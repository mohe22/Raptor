#pragma once

#include "core/session/base.hpp"
#include "libs/net/include/address.hpp"
#include <cstdint>
#include <shared_mutex>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "type.hpp"
#include "utils.hpp"

namespace Raptor::Core::Server {

    /**
     * @brief Session state snapshot for reporting to the UI.
     * Gets populated from Session::Base.
     */
    struct BriefSessionInfo {
        uint64_t    id;            /// Unique session id.
        Common::Types::ServerType protocol;  /// Protocol type (TCP, UDP, etc.).
        Session::Status status;    /// Current lifecycle status.
        uint64_t    idleSeconds;   /// Seconds since last received data.
        std::string remoteAddress; /// Remote ip:port as a human-readable string.
        std::string hostname;      /// Agent hostname.
        std::string username;      /// Agent username.
        std::string os;            /// Agent operating system.
        std::string timezone;      /// Agent timezone.
    };

    using SessionsInfoList = std::vector<BriefSessionInfo>;

    /**
     * @brief Owns and manages all active sessions regardless of protocol type.
     *
     * Thread-safe via shared_mutex — multiple readers allowed simultaneously,
     * writes (add/remove) are exclusive.
     *
     * Ownership model:
     *   SessionManager owns sessions via unique_ptr<Base>.
     *   Everything else (epoll, handlers) holds raw non-owning pointers.
     *   Destroying a session automatically closes its socket.
     */
    class SessionManager {
    public:
        SessionManager();
        ~SessionManager();

        SessionManager(const SessionManager&)            = delete;
        SessionManager& operator=(const SessionManager&) = delete;
        SessionManager(SessionManager&&)                 = delete;
        SessionManager& operator=(SessionManager&&)      = delete;

        /**
         * @brief Takes ownership of a session and registers it.
         *
         * Template deduces the concrete type — returns T* directly
         * so the caller gets the right type without casting.
         *
         * @tparam T   Concrete session type (TcpSession, UdpSession, etc.)
         *             Must inherit from Session::Base.
         * @param session  Owning pointer to the session being registered.
         * @return         Raw non-owning pointer to the registered session.
         *
         * @code
         *   auto session = std::make_unique<TcpSession>(std::move(conn));
         *   TcpSession* raw = manager.add(std::move(session));
         *   epoll_.add(raw);
         * @endcode
         */
        template<typename T>
        T* add(std::unique_ptr<T> session) {
            static_assert(std::is_base_of_v<Session::Base, T>, "T must inherit from Session::Base");
            T* raw = session.get();
            {
                std::unique_lock lock(mutex_);
                sessions_.emplace(raw->id(), std::move(session));
            }
            onSessionCreated(raw->id(), raw->getAddressStr(), raw->type());
            return raw;
        }

        /**
         * @brief Returns an existing session matching the address or creates a new one.
         *
         * Lookup is performed by ip:port string. If no match is found, a new session
         * is constructed and registered. Reused sessions are not logged.
         *
         * @tparam T       Concrete session type. Must inherit from Session::Base.
         * @param address  Remote address to look up or bind to the new session.
         * @return         Raw non-owning pointer, or nullptr if the address is invalid.
         */
        template<typename T>
        T* getOrCreate(const Net::Address& address) {
            static_assert(std::is_base_of_v<Session::Base, T>, "T must inherit from Session::Base");
            auto ip   = address.getIp();
            auto port = address.getPort();
            // should never happen address is validated before reaching here
            [[unlikely]] if (!ip || !port) {
                onSessionCreateFailed();
                return nullptr;
            }
            const std::string target = std::format("{}:{}", ip.value(), port.value());
            T* raw = nullptr;
            {
                std::unique_lock lock(mutex_);
                for (const auto& [sid, session] : sessions_)
                    if (session->getAddressStr() == target)
                        return static_cast<T*>(session.get());
                auto session = std::make_unique<T>(std::move(address), Common::nextId());
                raw = static_cast<T*>(session.get());
                sessions_.emplace(session->id(), std::move(session));
            }
            onSessionCreated(raw->id(), target, raw->type());
            return raw;
        }

        /**
         * @brief Removes and destroys a session by id.
         *
         * IMPORTANT — always remove from epoll BEFORE calling this.
         * Destroying the session closes the socket; any raw pointer held
         * after this call will dangle.
         *
         * Destruction chain:
         *   unique_ptr<Base> destructs
         *     → ConcreteSession destructs
         *       → socket closed
         *
         * @param id  Session id returned by Base::id().
         * @return    true if found and removed, false if id did not exist.
         *
         * @code
         *   epoll_.closeDescriptor(raw);
         *   manager.remove(session->id());
         * @endcode
         */
        bool remove(uint64_t id);

        /**
         * @brief Removes all sessions regardless of type.
         *
         * IMPORTANT — clear epoll before calling this to avoid dangling pointers.
         */
        void clear();

        /**
         * @brief Looks up a session by id.
         *
         * @param id  Session id to look up.
         * @return    Raw non-owning pointer, or nullptr if not found.
         *
         * @code
         *   Session::Base* s = manager.find(id);
         *   if (!s) // session already removed
         * @endcode
         */
        Session::Base* find(uint64_t id) const;

        /**
         * @brief Returns true if a session with the given id is registered.
         *
         * @param id  Session id to check.
         */
        bool contains(uint64_t id) const;

        /**
         * @brief Returns the total number of active sessions.
         */
        [[nodiscard]] size_t count() const noexcept;

        /**
         * @brief Returns true if there are no active sessions.
         */
        [[nodiscard]] bool empty() const noexcept;

        /**
         * @brief Iterates over all sessions regardless of type.
         *
         * Do not call add() or remove() from within fn — deadlock.
         *
         * @tparam Fn  Callable type receiving Session::Base*.
         * @param  fn  Called once per session with a raw non-owning pointer.
         *
         * @code
         *   manager.forEach([](Session::Base* s) { s->print(); });
         * @endcode
         */
        template<typename Fn>
        void forEach(Fn&& fn) const {
            std::shared_lock lock(mutex_);
            for (const auto& [_, session] : sessions_)
                fn(session.get());
        }

        /**
         * @brief Iterates over sessions of a specific concrete type.
         *
         * Safely skips sessions that are not of type T via dynamic_cast.
         *
         * @tparam T   Concrete session type. Must inherit from Session::Base.
         * @tparam Fn  Callable type receiving T*.
         * @param  fn  Called once per matching session with a typed pointer.
         *
         * @code
         *   manager.forEachOf<TcpSession>([](TcpSession* s) {
         *       s->enqueue(heartbeat, len);
         *   });
         * @endcode
         */
        template<typename T, typename Fn>
        void forEachOf(Fn&& fn) const {
            static_assert(std::is_base_of_v<Session::Base, T>, "T must inherit from Session::Base");
            std::shared_lock lock(mutex_);
            for (const auto& [id, session] : sessions_)
                if (auto* ptr = static_cast<T*>(session.get()))
                    fn(ptr);
        }

        /**
         * @brief Returns a snapshot of all sessions for API/UI reporting.
         *
         * @return  List of BriefSessionInfo structs, one per active session.
         */
        SessionsInfoList getSessionsInfo() const;

    private:
        /// Owns all sessions — key is session id, value is the session itself.
        std::unordered_map<uint64_t, std::unique_ptr<Session::Base>> sessions_;

        mutable std::shared_mutex   mutex_;    /// Protects sessions_ — shared reads, exclusive writes.
        std::thread thread_;   /// Background monitor thread.
        std::atomic<bool>  running_;  /// Controls monitor loop lifetime.
        std::condition_variable_any cv_;       /// Wakes monitor early on shutdown.

        /// Logs a new session creation with id, address, and protocol.
        void onSessionCreated(uint64_t id, const std::string& address, Common::Types::ServerType type) noexcept;

        /// Logs a failed session creation due to an invalid address.
        void onSessionCreateFailed() noexcept;

        /// Starts the background monitor thread.
        void startMonitor();

        /// Signals and joins the monitor thread.
        void stopMonitor();

        /// Scans sessions for idle/disconnected state and removes stale ones.
        void monitorLoop();
    };

} // namespace Raptor::Core::Server
