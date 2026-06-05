#pragma once

#include "core/servers/base.hpp"
#include "libs/net/include/server.hpp"

#include "core/servers/tcp/handlers/error.hpp"
#include "core/servers/tcp/handlers/incoming.hpp"
#include "core/servers/tcp/handlers/request.hpp"
#include "core/servers/tcp/handlers/response.hpp"
#include "core/servers/tcp/handlers/shutdown.hpp"
#include "core/servers/tcp/handlers/timeout.hpp"


namespace Raptor::Core::Servers {
    struct TcpCallbacks
    {
        /**
            * @brief Called when a new connection is accepted.
            *
            * The listener has already called accept() and obtained the raw client socket.
            * This callback wraps it in a Connection object so the upper layer can attach
            * any per-connection state (TLS context, HTTP parser, etc.) before the session
            * is registered with epoll.
            *
            * @param server  The listening TCP socket — use to call accept() or inspect
            *                the local address.
            * @return        Owning pointer to the wrapped Connection, or nullptr to reject.
            */
        std::function<Net::Result<std::unique_ptr<Net::Connection>>(Net::Servers::Tcp*)> incoming;

        /**
            * @brief Called when data is ready to be read from a session (EPOLLIN).
            *
            * In edge-triggered mode this is called in a loop until WouldBlock or 0.
            * In level-triggered mode it is called once per epoll notification.
            *
            * @param session  The readable client session.
            * @return         Number of bytes consumed, or an error.
            *                 WouldBlock → rearm and continue.
            *                 ConnectionClosed → close the session.
            *                 Any other error → report via `error` callback then close.
            */
        std::function<Net::Result<std::size_t>(Core::Session::TcpSession*)> request;

        /**
            * @brief Called when the socket is writable and the session has pending data (EPOLLOUT).
            *
            * Only fires when EPOLLOUT is set in the session's epoll mask, which happens
            * automatically when hasPendingWrites() is true at registration or rearm time.
            *
            * @param session  The writable client session.
            * @return         Number of bytes written, or an error.
            *                 WouldBlock → rearm and retry on next EPOLLOUT.
            *                 ConnectionClosed → close the session.
            *                 Any other error → report via `error` callback then close.
            */
        std::function<Net::Result<std::size_t>(Core::Session::TcpSession*)> response;

        /**
            * @brief Called when a session is shutting down gracefully.
            *
            * Invoked from the onClose handler before the session is removed from the
            * session manager. Use this to flush logs, release per-session resources, or
            * notify the application layer that the Session disconnected.
            *
            * @param session  The session that is about to be destroyed.
            */
        std::function<void(Core::Session::TcpSession*)> shutdown;

        /**
            * @brief Called when an unrecoverable error occurs on a session.
            *
            * The listener will close the session immediately after this callback returns.
            * Do not call epoll_.closeSession() from within this callback.
            *
            * @param session  The affected session.
            * @param error    The error reported by the OS or epoll.
            */
        std::function<void(Core::Session::TcpSession*, const Net::Error&)> error;

        /**
            * @brief Called on every epoll_wait() timeout when no events are pending.
            *
            * The listener rearms all active sessions before invoking this callback.
            * Useful for heartbeat checks, idle timeout enforcement, or metrics emission.
            */
        std::function<void()> timeout;

        /**
            * @brief Factory: create a Callbacks set wired to the built-in default handlers.
            *
            * The default handlers log events to stdout and perform no protocol logic.
            * Suitable as a starting point or for testing the listener in isolation.
            */
        static TcpCallbacks makeDefault()
        {
            TcpCallbacks t;
            t.error    = Tcp::Handlers::handleError;
            t.incoming = Tcp::Handlers::handleIncoming;
            t.request  = Tcp::Handlers::handleRequest;
            t.response = Tcp::Handlers::handleResponse;
            t.shutdown = Tcp::Handlers::handleShutdown;
            t.timeout  = Tcp::Handlers::handleTimeout;
            return t;
        }
    };
    /**
        * @brief TCP server implementation using epoll-based event dispatching.
        *
        * Combines the low-level TCP socket server with the epoll event
        * manager to handle asynchronous network events efficiently.
        */
    class TcpServer : public Base {
        public:

        /**
            * @brief Constructs a TCP server instance.
            *
            * @param config Server configuration settings.
            * @param name   Human-readable server name.
            */
        TcpServer(const ServerConfig& config)
        : Base(config, Common::Types::ServerType::TCP)
        , callbacks_(TcpCallbacks::makeDefault()) {}

        ~TcpServer() noexcept override {
            std::println("~TcpServer ({})", config().instanceName);
            stop();
        }

        /**
            * @brief Pauses the server event loop.
            *
            * Suspends accepting and processing socket events
            * without fully shutting down the server.
            */
        void pause() noexcept override {
            changeStatus(ServerStatus::Paused);
            server_.pause();
            epoll_.pause();
        }

        /**
            * @brief Resumes the server event loop.
            *
            * Re-enables accepting and processing socket events
            * after a previous pause operation.
            */
        void resume() noexcept override {
            changeStatus(ServerStatus::Running);
            server_.resume();
            epoll_.resume();
        }

        /**
            * @brief Stops the server and shuts down the worker thread.
            *
            * Stops the TCP server, terminates the epoll loop,
            * and waits for the worker thread to exit safely.
            *
        */
        void stop() noexcept override {
            changeStatus(ServerStatus::Stopped);
            epoll_.stop();
            server_.stop();
        }


        bool isRunning() const noexcept  {
            return Base::isRunning() && !epoll_.isStopped();
        }

        bool isPaused() const noexcept  {
            return epoll_.isPaused();
        }

        bool isStopped() const noexcept  {
            return Base::isStopped() || epoll_.isStopped();
        }



        protected:

        /**
            * @brief Initialise the listener — create socket, bind, listen, set up epoll.
            *
            */
        bool init() noexcept override;


        /**
            * @brief Start the main event loop — blocks until stop() or a fatal error.
            *
            * Captures the kernel TID via captureThreadId() before entering the loop so
            * that getResources() always reads the correct /proc/self/task/[tid] entries,
            * even when called from an external monitor thread.
            *
            */
        bool run() noexcept override;

        private:

        /// Manages epoll event polling and descriptor dispatching.
        EpollManager epoll_;

        /// Underlying TCP socket server responsible for bind/listen/accept.
        Net::Servers::Tcp server_;

        /// Application-level session and event callbacks.
        TcpCallbacks callbacks_;

        void destroyClient(Session::TcpSession* client) noexcept;
        void closeClient  (Session::TcpSession* client, const Net::Error& err) noexcept;
        bool rearmClient  (Session::TcpSession* client) noexcept;
        void onAccept  () noexcept;
        void onRead    (Session::TcpSession* client) noexcept;
        void onWrite   (Session::TcpSession* client) noexcept;
        void onTimeout () noexcept;

    };

} // namespace Raptor::Core::Servers
