



#include <arpa/inet.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <netinet/in.h>
#include <print>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <cstdint>
#include <fcntl.h>
#include <thread>
#include <vector>

#include "./utils.hpp"
#include "common/header.hpp"
#include "common/parser/tcp.hpp"
#include "pips.hpp"
#include "common/register.hpp"
constexpr uint32_t READ_FLAGS  = EPOLLIN  | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
constexpr uint32_t WRITE_FLAGS = EPOLLOUT | EPOLLET | EPOLLONESHOT;

constexpr size_t SEND_BUF_SIZE = 4096;
constexpr size_t RECV_BUF_SIZE = 4096;
constexpr size_t FILE_CHUNK_SIZE = 4096;
constexpr size_t FLUSH_THRESHOLD = 1500;

using namespace Raptor;
template<size_t N>
struct Buffer {
    char   data[N]{};
    size_t offset{0};
    size_t len{0};

    void reset() { offset = 0; len = 0; }
    bool yisEmpty() const { return offset >= len; }
    size_t pending() const { return len - offset; }
};

struct CommandEntry {
    uint32_t taskId{0};

    char   stdinBuf[RECV_BUF_SIZE]{};
    size_t stdinLen{0};
    size_t stdinOffset{0};

    char   outBuf[SEND_BUF_SIZE]{};
    size_t outLen{0};
    size_t outOffset{0};

    bool done{false};

    bool stdinEmpty()   const { return stdinOffset >= stdinLen; }
    size_t stdinPending() const { return stdinLen - stdinOffset; }
    bool outEmpty() const { return outLen == 0 || outOffset >= Common::Header::SIZE + outLen; }
    size_t outPending() const { return outLen == 0 ? 0 : Common::Header::SIZE+ outLen - outOffset; }
};



class Agent : public Common::Parsers::TcpParser<uint8_t, sizeof(Common::Register)>
{

    public:
    Agent(const std::string& ip, uint16_t port)
    : ip_(ip), port_(port)
    {
        epollfd = ::epoll_create1(0);
        if (epollfd == -1)
            throw std::runtime_error(std::format("epoll_create1: {}", strerror(errno)));

        pipfds = Pip::create();
        setNonBlocking(pipfds.err);
        setNonBlocking(pipfds.out);
        setNonBlocking(pipfds.in);

        addEpoll(pipfds.in, EPOLLERR | EPOLLHUP);
        addEpoll(pipfds.out, READ_FLAGS);
        addEpoll(pipfds.err, READ_FLAGS);
        connect();

    }
    ~Agent() override = default;

    void closeClient() {
        if (clientfd != -1) {
            ::epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, nullptr);
            ::close(clientfd);
            clientfd = -1;
        }
    }


    bool connect() {
          closeClient();
          while (true) {
              clientfd = ::socket(AF_INET, SOCK_STREAM, 0);
              if (clientfd == -1) {
                  std::println(stderr, "reconnect: socket(): {}", strerror(errno));
                  continue;
              }

              sockaddr_in addr{};
              addr.sin_family = AF_INET;
              addr.sin_port   = htons(port_);
              if (::inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) <= 0) {
                  std::println(stderr, "reconnect: inet_pton({}): invalid", ip_);
                  ::close(clientfd); clientfd = -1;
                  continue;
              }

              setNonBlocking(clientfd);

              int rc = ::connect(clientfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
              if (rc == -1 && errno != EINPROGRESS && errno != EWOULDBLOCK) {
                  std::println(stderr, "reconnect: connect({}:{}): {}", ip_, port_, strerror(errno));
                  ::close(clientfd); clientfd = -1;
                  continue;
              }

              if (!addEpoll(clientfd, READ_FLAGS)) {
                  std::println(stderr, "reconnect: addEpoll failed: {}", strerror(errno));
                  ::close(clientfd); clientfd = -1;
                  continue;
              }
              registerAgent();

              std::this_thread::sleep_for(std::chrono::seconds(30));
              return true;
          }
      }
private:
    std::string ip_;
    uint16_t port_;


    Pip pipfds{};
    Pty ptyfds{};

    static constexpr int MAX_EVENTS = 12;
    epoll_event epollEvents[MAX_EVENTS]{};

    int epollfd,clientfd;

    void rearm(int fd, uint32_t flags) noexcept {
        struct epoll_event ev{};
        ev.data.fd = fd;
        ev.events  = flags;
        epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
    }
    bool addEpoll(int fd, uint32_t events) noexcept {
        struct epoll_event ev{};
        ev.data.fd = fd;
        ev.events  = events;
        return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) != -1;
    }
    void setNonBlocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            throw std::runtime_error("fcntl F_GETFL failed");
        }

        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            throw std::runtime_error("fcntl F_SETFL failed");
        }
    }

    void registerAgent() {
        const auto reg = collect();
        const auto payload = reg.serialize();

        Common::Header header;
        header.packetId = 1;
        header.type= Common::PacketType::Register;
        header.direction = Common::PacketDirection::Resp;
        header.flags = Common::Flags::Success | Common::Flags::Text;
        header.payloadSize = sizeof(Common::Register);

        const auto headerBuf = header.serialize();

        std::vector<uint8_t> packet;
        packet.reserve(headerBuf.size() + payload.size());
        packet.insert(packet.end(), headerBuf.begin(), headerBuf.end());
        packet.insert(packet.end(), payload.begin(),   payload.end());

        ssize_t sent = ::write(clientfd, packet.data(), packet.size());
         if (sent < 0) {
             // reconnect();
             return;
         }
         std::println("{}",sent);
    }



     void onCommand(const Common::Header&, std::string_view)  noexcept override {}
     void onUpload(const Common::Header&, std::string_view)   noexcept override {}
     void onDownload(const Common::Header&, std::string_view) noexcept override {}


};

int main(){
    Agent a("127.0.0.1",8080);

}
