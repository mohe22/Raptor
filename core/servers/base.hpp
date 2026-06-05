#pragma once

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <thread>

#include "core/session/manager.hpp"
#include "libs/net/include/epoll.hpp"
#include "libs/net/include/types.hpp"
#include "core/session/tcp.hpp"

namespace Raptor::Core::Servers {

    /**
     * @brief Basic server runtime configuration.
     */
    struct ServerConfig {

        /// Listening IP address.
        char ip[INET6_ADDRSTRLEN] = "0.0.0.0";

        /// Listening TCP port.
        uint16_t port{8080};

        /// IP protocol version (IPv4 / IPv6).
        Net::IPType ipType{Net::IPType::IPv4};

        /// epoll_wait timeout in milliseconds.
        int epollTimeout{2000};

        std::string instanceName;
        static bool isValid(const ServerConfig& config) noexcept {
            if (config.instanceName.empty())
                return false;

            if (config.port == 0)
                return false;

            if (config.epollTimeout <= 0)
                return false;

            if (config.ip[0] == '\0')
                return false;

            if (config.ipType == Net::IPType::IPv4) {
                struct sockaddr_in sa{};
                if (inet_pton(AF_INET, config.ip, &sa.sin_addr) != 1)
                    return false;
            } else {
                struct sockaddr_in6 sa{};
                if (inet_pton(AF_INET6, config.ip, &sa.sin6_addr) != 1)
                    return false;
            }

            return true;
        }
        std::string toJsonStr() const noexcept {
            return std::format(
                R"({{"name":"{}","ip":"{}","port":{},"ipType":"{}","epollTimeout":{}}})",
                instanceName,
                ip,
                port,
                ipType == Net::IPType::IPv4 ? "IPv4" : "IPv6",
                epollTimeout
            );
        }
    };

    /**
     * @brief Current runtime state of the server.
     */
    enum class ServerStatus : uint8_t {
        Running = 0,
        Paused  = 1,
        Stopped = 2,
        Error   = 3,
    };
    inline const char* ToString(ServerStatus status) noexcept {
        switch (status) {
            case ServerStatus::Running: return "running";
            case ServerStatus::Paused:  return "paused";
            case ServerStatus::Stopped: return "stopped";
            case ServerStatus::Error:   return "error";
            default:                    return "unknown";
        }
    }

    /**
     * @brief Thin wrapper around the epoll watcher.
     *
     * Handles descriptor registration, event dispatching,
     * and epoll lifecycle management.
     */
    class EpollManager {
    public:

        EpollManager(const EpollManager&)            = delete;
        EpollManager& operator=(const EpollManager&) = delete;
        EpollManager(EpollManager&&)                 = delete;
        EpollManager& operator=(EpollManager&&)      = delete;

        EpollManager()  = default;
        ~EpollManager() = default;

        /**
         * @brief Initializes the epoll watcher.
         *
         * @param timeout epoll wait timeout in milliseconds.
         * @return Result<void> Success or error state.
         */
        Net::Result<void> init(const int timeout) noexcept {
            close();

            auto w = Net::Poll::Watcher::create(timeout);

            if (!w)
                return std::unexpected{w.error()};

            watcher_ = std::move(w.value());

            return {};
        }

        /**
         * @brief Registers a generic descriptor with epoll.
         */
        template<Net::Poll::Watchable T>
        Net::Result<void> add(T* desc) noexcept {
            return watcher_.add(desc, baseEvents());
        }

        /**
         * @brief Registers a TCP Session with dynamic events.
         */
        Net::Result<void> add(Session::TcpSession* Session) noexcept {
            return watcher_.add(Session, eventsFor(Session));
        }

        /**
         * @brief Starts the blocking epoll event loop.
         */
        Net::Result<void> watch() noexcept {
            return watcher_.watch();
        }

        /// Pauses event processing.
        void pause() noexcept { watcher_.pause(); }

        /// Resumes event processing.
        void resume() noexcept { watcher_.resume(); }

        /// Stops the epoll loop.
        void stop() noexcept { watcher_.stop(); }

        /**
         * @brief Closes the watcher and releases epoll resources.
         */
        void close() noexcept {
            watcher_.close();
            watcher_ = Net::Poll::Watcher{};
        }

        /**
         * @brief Returns the default event mask.
         */
        [[nodiscard]] Net::EpollEvent baseEvents() const noexcept {
            Net::EpollEvent ev =
                Net::EpollEvent::EPOLLIN   |
                Net::EpollEvent::EPOLLERR  |
                Net::EpollEvent::EPOLLHUP  |
                Net::EpollEvent::EPOLLRDHUP |
                Net::EpollEvent::EPOLLET;

            return ev;
        }

        /**
         * @brief Returns the event mask for a TCP Session.
         *
         * Adds EPOLLOUT when pending writes exist.
         */
        [[nodiscard]] Net::EpollEvent eventsFor(const Session::TcpSession* Session) const noexcept {
            auto ev = baseEvents();

            if (Session->hasPendingWrites())
                ev |= Net::EpollEvent::EPOLLOUT;

            return ev;
        }

        [[nodiscard]] bool isPaused()  const noexcept { return watcher_.isPaused(); }
        [[nodiscard]] bool isStopped() const noexcept { return watcher_.isStopped(); }

        /// Registers read callback.
        void onRead(std::function<void(Net::Poll::Descriptor*)> cb) {
            watcher_.onRead(std::move(cb));
        }

        /// Registers write callback.
        void onWrite(std::function<void(Session::TcpSession*)> cb) {
            watcher_.onWrite(std::move(cb));
        }

        /// Registers timeout callback.
        void onTimeout(std::function<void()> cb) {
            watcher_.onTimeout(std::move(cb));
        }

        /**
         * @brief Registers error callback.
         */
        template<Net::Poll::Watchable T>
        void onError(std::function<void(T*, const Net::Error&)> cb) {
            watcher_.onError<T>(std::move(cb));
        }

        /**
         * @brief Registers close callback.
         */
        template<Net::Poll::Watchable T>
        void onClose(std::function<void(T*)> cb) {
            watcher_.onClose<T>(std::move(cb));
        }

        /**
         * @brief Closes and removes a descriptor.
         */
        void closeDescriptor(Net::Poll::Descriptor* desc) noexcept {
            watcher_.close(desc);
        }

        /**
         * @brief Rearms epoll events for a TCP Session.
         */
        Net::Result<void> rearm(Session::TcpSession* Session) noexcept {
            return watcher_.mod(Session, eventsFor(Session));
        }

    private:

        /// Underlying epoll watcher implementation.
        Net::Poll::Watcher watcher_;
    };

    /**
     * @brief Lightweight server error container.
     */
    struct ServerError {

        /// Native/system error code.
        std::atomic<int> code{0};

        /// Human-readable error message.
        char message[256]{0};

        /// Returns true if an error exists.
        bool has() const noexcept {
            return code != 0;
        }

        /**
         * @brief Stores an error code and message.
         */
        void set(int c, const char* msg) noexcept {
            code = c;

            strncpy(message, msg, sizeof(message) - 1);

            message[sizeof(message) - 1] = '\0';
        }

        /// Clears the current error state.
        void clear() noexcept {
            code = 0;
            message[0] = '\0';
        }
    };





    /**
     * @brief Abstract base class for all server implementations.
     */
    class Base {
    public:

        /**
         * @brief Constructs the server base object.
         */
        explicit Base(const ServerConfig& config, Common::Types::ServerType type)
            : config_(config), type_(type){};
        virtual ~Base() = default;

        Base(const Base&)    = delete;
        Base& operator=(const Base&) = delete;
        Base(Base&&) = delete;
        Base& operator=(Base&&)= delete;

        bool hasError() const noexcept {
            return error.has();
        }
        const std::string_view getErrorMsg() const noexcept {
            return error.message;
        }
        const std::string getErrorMsgStr() const noexcept {
            return std::string(error.message);
        }
        const int getErrorCode() const noexcept {
            return error.code;
        }

        /**
         * @brief Starts the server worker thread.
         *
         * Initializes resources and launches the server loop.
         */
        bool start() noexcept {

            if (!init()) {
                // Initialization failed.
                if (error.has()) {
                    status_.store(ServerStatus::Error, std::memory_order_release);
                }
                return false;
            }


            status_.store(ServerStatus::Running, std::memory_order_release);
            startTime_ = Common::Types::Clock::now();
            thread_ = std::jthread([this]() {
                if (!run()) {
                    if (error.has()) {
                        status_.store(ServerStatus::Error, std::memory_order_release);
                    }
                }
            });
            return true;
        }

        virtual void pause() noexcept = 0;
        virtual void resume() noexcept = 0;
        virtual void stop() noexcept = 0;


        /**
         * @brief Returns the total uptime since the server was started.
         *
         * Pausing does not affect uptime — the clock keeps running.
         */
        [[nodiscard]] Common::Types::Clock::duration uptime() const noexcept {
            return Common::Types::Clock::now() - startTime_;
        }
        [[nodiscard]] uint64_t uptimeSeconds() const noexcept {
            return std::chrono::duration_cast<std::chrono::seconds>(uptime()).count();
        }
        /**
         * @brief Updates the server runtime state.
         */
        void changeStatus(const ServerStatus newStatus) noexcept {
            status_.store(newStatus, std::memory_order_release);
        }

        // In class Base
        [[nodiscard]] bool isRunning() const noexcept {
            return status() == ServerStatus::Running;
        }

        [[nodiscard]] bool isPaused() const noexcept {
            return status() == ServerStatus::Paused;
        }

        [[nodiscard]] bool isStopped() const noexcept {
            return status() == ServerStatus::Stopped;
        }

        /// Returns server configuration.
        const ServerConfig& config() const noexcept { return config_; }


        /// Returns the current runtime state.
        [[nodiscard]] ServerStatus status() const noexcept {
            return status_.load(std::memory_order_acquire);
        }
        void updateInstanceName(const std::string& name) noexcept {
            config_.instanceName = name;
        }

        /**
            * @brief Waits for the worker thread to finish.
            *
            * Ensures clean shutdown synchronization before
            * destroying resources used by the server thread.
            */
           void join() noexcept {
               if (thread_.joinable()) {
                   thread_.join();
               }
           }

           void getRxBytes(uint64_t& n) const noexcept { n = rxBytes_.load(std::memory_order_relaxed); }
           void getTxBytes(uint64_t& n) const noexcept { n = txBytes_.load(std::memory_order_relaxed); }
           const Common::Types::ServerType type() const noexcept { return type_; }
           Server::SessionManager sessionManager;
    protected:

           /// Initializes server resources.
        virtual bool init() noexcept = 0;

        /// Main blocking server loop.
        virtual bool run() noexcept = 0;

        /// Internal server error state.
        ServerError error;




        void addRxBytes(uint64_t n) noexcept { rxBytes_.fetch_add(n, std::memory_order_relaxed); }
        void addTxBytes(uint64_t n) noexcept { txBytes_.fetch_add(n, std::memory_order_relaxed); }


    private:


        /// Runtime configuration.
        ServerConfig config_;

        /// Background worker thread.
        std::jthread thread_;


        Common::Types::TimePoint startTime_;

        /// Current server status.
        std::atomic<ServerStatus> status_{ServerStatus::Paused};

        std::atomic<uint64_t> rxBytes_{0};
        std::atomic<uint64_t> txBytes_{0};

        Common::Types::ServerType type_;


    };

} // namespace Raptor::Core::Servers
