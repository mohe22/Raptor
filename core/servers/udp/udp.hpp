#pragma once

#include "core/servers/base.hpp"
#include "core/session/udp.hpp"
#include "libs/net/include/server.hpp"

#include "core/servers/udp/handlers/datagram.hpp"
#include "core/servers/udp/handlers/error.hpp"
#include "core/servers/udp/handlers/timeout.hpp"

namespace Raptor::Core::Servers {

    /**
     * @brief Callbacks for connectionless (UDP) listeners.
     *
     * Unlike TCP callbacks, there are no session lifecycle events (incoming, shutdown)
     * because UDP has no connections. Each datagram is independent.
     *
     * The socket reference passed to `datagram` allows the upper layer to send
     * replies directly via server.sendTo() — the listener itself never sends.
     */
    struct UdpCallbacks {
        /**
         * @brief Called when a complete datagram is received.
         *
         * @param buf     Pointer to the receive buffer containing the datagram payload.
         *                Valid only for the duration of this callback — copy if needed.
         * @param len     Number of bytes received.
         * @param session Pointer to the UDP session representing the sender.
         *                Used to reply via the session's sendTo() interface.
         *
         * @return The number of bytes to be sent as a reply, or 0 to discard the reply.
         * @note The listener imposes no reply policy. The callback may reply immediately,
         *       enqueue the datagram for a worker thread, or discard it entirely.
         */
        std::function<size_t(
            const uint8_t*  buf,
            const std::size_t     len,
            const Session::UdpSession*,
            const Net::Servers::Udp&
        )> datagram;

        /**
         * @brief Called when a fatal socket-level error occurs.
         *
         * Triggered on EPOLLERR / EPOLLHUP, or when recvfrom() returns an
         * error other than EAGAIN/EWOULDBLOCK. After this fires the listener
         * will stop — it is not a recoverable per-peer condition.
         *
         * @param error The error reported by epoll or recvfrom().
         */
        std::function<void(const Net::Error&)> error;

        /**
         * @brief Called on every epoll_wait() timeout when no datagrams are pending.
         *
         * Useful for periodic housekeeping — rate-limit tables, idle peer cleanup, etc.
         * The interval is controlled by config.epollTimeout passed to epoll_.init().
         */
        std::function<void()> timeout;

        /**
         * @brief Constructs a Callbacks instance wired to the default handlers.
         *
         * Default handlers provide basic logging behaviour. Replace individual
         * fields after construction to customise without overriding everything.
         */
        static UdpCallbacks makeDefault() {
            UdpCallbacks t;
            t.datagram = Udp::Handlers::handleDatagram;
            t.error    = Udp::Handlers::handleError;
            t.timeout  = Udp::Handlers::handleTimeout;
            return t;
        }
    };

    /**
     * @brief UDP server implementation using epoll-based event dispatching.
     *
     * Why epoll with UDP?
     * -------------------
     * UDP is connectionless — there is only ever one socket fd, shared by
     * every peer. epoll_wait() lets the worker thread sleep at zero CPU cost
     * until the kernel signals that datagrams are queued, then the run() loop
     * drains the entire receive buffer with back-to-back recvfrom() calls
     * before returning to epoll_wait(). This avoids two common pitfalls:
     *
     *   - Busy-poll loop  : would peg a core at 100% even when the socket is idle.
     *   - Blocking recvfrom() : would prevent timeout callbacks and clean shutdown.
     *
     * epoll also unifies UDP and TCP servers under the same EpollManager
     * interface, keeping the event dispatch path consistent across the codebase.
     *
     * Ownership model
     * ---------------
     * epoll_   owns the epoll fd and all registered descriptors.
     * server_  owns the UDP socket (bind/send/receive).
     * Both are stopped and joined in stop(), which is also called by the destructor,
     * so there is no teardown ordering issue.
     */
    class UdpServer : public Base {
    public:

        /**
         * @brief Constructs a UdpServer and installs the default callbacks.
         *
         * Does not open any sockets or file descriptors — call init() first,
         * then run() to enter the event loop.
         *
         * @param config Server configuration (IP, port, epoll timeout, etc.).
         */
        UdpServer(const ServerConfig& config)
        : Base(config, Common::Types::ServerType::UDP)
        , callbacks_(UdpCallbacks::makeDefault()) {}

        /**
         * @brief Destructor — ensures the server is fully stopped before teardown.
         *
         * Calls stop() so that the epoll loop exits and the worker thread is
         * joined before any members are destroyed.
         */
        ~UdpServer() noexcept override {
            std::println("~UdpServer ({})", config().instanceName);
            stop();
        }

        /**
         * @brief Suspends the epoll event loop without closing the socket.
         *
         * Incoming datagrams will accumulate in the kernel receive buffer
         * while paused. Call resume() to drain them.
         */
        void pause() noexcept override {
            changeStatus(ServerStatus::Paused);
            server_.pause();
            epoll_.pause();
        }

        /**
         * @brief Resumes the epoll event loop after a pause.
         *
         * Datagrams buffered by the kernel during the pause will be
         * delivered on the next epoll_wait() wake-up.
         */
        void resume() noexcept override {
            changeStatus(ServerStatus::Running);
            server_.resume();
            epoll_.resume();
        }

        /**
         * @brief Stops the server and joins the worker thread.
         *
         * Signals the epoll loop to exit
         */
        void stop() noexcept override {
            changeStatus(ServerStatus::Stopped);
            server_.stop();
            epoll_.stop();
        }

        /** @return true if the base status, epoll loop, and UDP socket are all running. */
        bool isRunning() const noexcept {
            return Base::isRunning() && !epoll_.isStopped() && !server_.isStopped();
        }

        /** @return true if the epoll loop, UDP socket, and base status are all paused. */
        bool isPaused() const noexcept {
            return epoll_.isPaused() && server_.isPaused() && Base::isPaused();
        }

        /** @return true if the epoll loop, UDP socket, and base status are all stopped. */
        bool isStopped() const noexcept {
            return Base::isStopped() && epoll_.isStopped() && server_.isStopped();
        }

    protected:

        /** @brief Creates the UDP socket and initialises the epoll instance. */
        bool init() noexcept override;

        /** @brief Registers the socket with epoll and enters the event dispatch loop. */
        bool run() noexcept override;

    private:
        /// Manages the epoll fd, descriptor registration, and event dispatch.
        /// Shared with UDP because there is only one socket fd to watch
        EpollManager epoll_;

        /// Underlying UDP socket — handles bind(), recvfrom(), and sendTo().
        Net::Servers::Udp server_;

        /// Application-level event callbacks (datagram, error, timeout).
        UdpCallbacks callbacks_;
    };

} // namespace Raptor::Core::Servers
