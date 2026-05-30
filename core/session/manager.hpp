#pragma once
#include <shared_mutex>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <memory>
#include "core/session/base.hpp"
#include "libs/net/include/address.hpp"
#include "utils.hpp"
#include "core/config.hpp"

namespace Raptor::Core::Server {
    inline bool operator==(
        const Net::Result<std::pair<std::string_view, uint16_t>>& res,
        const Net::Address& addr) noexcept
    {
        if (!res) return false;
        auto ip   = addr.getIp();
        auto port = addr.getPort();
        if (!ip || !port) return false;
        return res->first == ip.value() && res->second == port.value();
    }
    /**
     * @brief Owns and manages all active sessions regardless of protocol type.
     *
     * Thread-safe via shared_mutex — multiple threads can read simultaneously,
     * writes (add/remove) are exclusive.
     *
     * Ownership model:
     *   SessionManager owns sessions via unique_ptr<Base>
     *   Everything else (epoll, handlers) holds raw non-owning pointers
     *   Destroying a session automatically closes its socket
     */
    class SessionManager {
    public:

    SessionManager() {
        sessions_.reserve(Core::Config::MAX_CONNECTIONS);
        startMonitor();
    }

    ~SessionManager() {
        stopMonitor();
    }


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
         * @tparam T  Concrete session type (TcpSession, UdpSession, etc.)
         *            Must inherit from Session::Base.
         *
         * @code
         *   auto session = std::make_unique<TcpSession>(std::move(conn));
         *   TcpSession* raw = manager.add(std::move(session)); // returns TcpSession*
         *   epoll_.add(raw);
         * @endcode
         */

        template<typename T>
        T* add(std::unique_ptr<T> session) {
            static_assert(std::is_base_of_v<Session::Base, T>,"T must inherit from Session::Base");
            T* raw = session.get(); // get raw pointer to return
            const uint64_t id  = raw->id();
            std::unique_lock lock(mutex_);
            sessions_.emplace(id, std::move(session));
            return raw;
        }



        // Todo: change it later.
        template<typename T>
        T* getOrCreate(const Net::Address& address) {
            static_assert(std::is_base_of_v<Session::Base, T>, "T must inherit from Session::Base");
            if (auto* s = find(address))
                   return static_cast<T*>(s);
            auto session = std::make_unique<T>(std::move(address), Common::nextId());
            T* raw = session.get();
            auto id = raw->id();
            std::unique_lock lock(mutex_);
            sessions_.emplace(id, std::move(session));
            return raw;
        }


        /**
         * @brief Removes and destroys a session by id.
         *
         * IMPORTANT — always remove from epoll BEFORE calling this.
         * Destroying the session closes the socket. If epoll still holds
         * the raw pointer after this call it will dangle.
         *
         * Destruction chain:
         *   unique_ptr<Base> destructs
         *     → TcpSession destructs
         *       → unique_ptr<Connection> destructs
         *         → socket closed
         *
         * @param id  Session id returned by Base::id().
         * @return    true if found and removed, false if id did not exist.
         *
         * @code
         *   epoll_.closeDescriptor(raw);   // remove from epoll FIRST
         *   manager.remove(session->id()); // then destroy
         * @endcode
         */
        bool remove(const uint64_t id) {
            std::unique_lock lock(mutex_);
            return sessions_.erase(id) > 0;
        }



        /**
         * @brief Removes all sessions regardless of type.
         *
         * IMPORTANT — clear epoll before calling this.
         */
        void clear() {
            std::unique_lock lock(mutex_);
            sessions_.clear();
        }
        /**
         * @brief Looks up a session by id.
         *
         * @param id  Session id.
         * @return    Raw non-owning pointer, or nullptr if not found.
         *
         * @code
         *   Session::Base* s = manager.find(id);
         *   if (!s) // session already removed
         * @endcode
         */
        Session::Base* find(const uint64_t id) const {
            std::shared_lock lock(mutex_);
            auto it = sessions_.find(id);
            return it != sessions_.end() ? it->second.get() : nullptr;
        }
        Session::Base* find(const Net::Address& address) const {
            std::shared_lock lock(mutex_);
            for (const auto& [id, session] : sessions_)
                if (session->getAddressStr() == address)
                    return session.get();
            return nullptr;
        }
        /**
         * @brief Checks if a session exists.
         *
         * @param id  Session id.
         * @return    true if session is registered.
         */
        bool contains(const uint64_t id) const {
            std::shared_lock lock(mutex_);
            return sessions_.contains(id);
        }

        /**
         * @brief Returns total number of active sessions across all types.
         */
        [[nodiscard]] size_t count() const noexcept {
            std::shared_lock lock(mutex_);
            return sessions_.size();
        }

      /**
         * @brief Returns true if there are no active sessions.
         */
        [[nodiscard]] bool empty() const noexcept {
            std::shared_lock lock(mutex_);
            return sessions_.empty();
        }

        /**
         * @brief Iterates over ALL sessions regardless of type.
         *
         * Useful for heartbeat checks, metrics, broadcast.
         * Do not call add() or remove() from within fn — deadlock.
         *
         * @param fn  Callable receiving Session::Base*
         *
         * @code
         *   manager.forEach([](Session::Base* s) {
         *       if (s->idleSeconds() > 60)
         *           // kick idle session
         *   });
         * @endcode
         */
        template<typename Fn>
        void forEach(Fn&& fn) const {
            std::shared_lock lock(mutex_);
            for (const auto& [id, session] : sessions_)
                fn(session.get());
        }

        /**
         * @brief Iterates over sessions of a specific concrete type.
         *
         * Casts to T* for you — only call with the matching Session::Type.
         * Do not call add() or remove() from within fn — deadlock.
         *
         * @tparam T     Concrete session type (TcpSession, UdpSession, etc.)
         * @param  type  Must match T (e.g. T=TcpSession, type=Type::TCP)
         * @param  fn    Callable receiving T*
         *
         * @code
         *   manager.forEachOf<TcpSession>(Session::Type::TCP, [](TcpSession* s) {
         *       s->enqueue(heartbeat, len);
         *   });
         * @endcode
         */
        template<typename T, typename Fn>
        void forEachOf(const Session::Type type, Fn&& fn) const {
            std::shared_lock lock(mutex_);
            for (const auto& [id, session] : sessions_)
                if (session->type() == type)
                    fn(static_cast<T*>(session.get()));
        }


    private:

        /// Owns all sessions — key is session id, value is the session itself.
        std::unordered_map<uint64_t, std::unique_ptr<Session::Base>> sessions_;

        /// Shared for reads, exclusive for writes.
        mutable std::shared_mutex mutex_;

        std::thread thread_;
        std::atomic<bool> running_{false};

        void startMonitor() {
            running_ = true;
            thread_  = std::thread([this] { monitorLoop(); });
        }

        void stopMonitor() {
            running_ = false;
            if (thread_.joinable())
                thread_.join();
        }

        void monitorLoop() {
            while (running_) {
                if (!running_) break;

                std::vector<uint64_t> toRemove;
                {
                    std::shared_lock lock(mutex_);
                    for (const auto& [id, session] : sessions_) {
                        const auto idle = session->idleSeconds();

                        if (session->isDisconnected()) {
                            toRemove.push_back(id);
                            continue;
                        }

                        if (idle >= Config::TIMEOUT_SECONDS && !session->isDisconnected()) {
                            // std::println("[Monitor] id={} idle={}s → Disconnected (timeout)",
                                         // id, idle);
                            session->setStatus(Session::Status::Disconnected);
                            toRemove.push_back(id);
                            continue;
                        }

                        if (idle >= Config::IDLE_SECONDS && session->isConnected()) {
                            // std::println("[Monitor] id={} idle={}s → Idle", id, idle);
                            session->setStatus(Session::Status::Idle);
                            continue;
                        }

                        // still active — just print stats
                        // std::println("[Monitor] id={} status={} idle={}s uptime={}s",
                        //              id,
                        //              Session::ToString(session->status()),
                        //              idle,
                        //              session->uptimeSeconds());
                    }
                }

                if (!toRemove.empty()) {
                    std::unique_lock lock(mutex_);
                    for (const auto id : toRemove) {
                        sessions_.erase(id);
                        // std::println("[Monitor] id={} removed", id);
                    }
                }
            }

        }
    };

} // namespace Raptor::Core::Server
