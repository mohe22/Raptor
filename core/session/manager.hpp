#pragma once

#include "core/session/base.hpp"
#include "core/config.hpp"
#include "libs/net/include/address.hpp"
#include <cstdint>
#include <shared_mutex>
#include <mutex>
#include <sys/types.h>
#include <unordered_map>
#include "type.hpp"
#include "utils.hpp"

namespace Raptor::Core::Server {


    /**
     * @brief session's state for reporting to the UI.
     *
     * Gets populated from Session::Base
     */
    struct BriefSessionInfo {
        uint64_t id;  ///< Unique session id.
        Common::Types::ServerType protocol;       ///< Protocol type (TCP, UDP, etc.).
        Session::Status status;         ///< Current lifecycle status.
        uint64_t  idleSeconds;    ///< Seconds since last received data.
        std::string remoteAddress;  ///< Remote ip:port as a human-readable string.
        std::string hostname; ///< agent hostname;
        std::string username; //< agent username;
        std::string os; ///< the agent operating sys
        std::string timezone; //< agent time zone
    };
    using SessionsInfoList = std::vector<BriefSessionInfo>;



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



        template<typename T>
        T* getOrCreate(const Net::Address& address) {
            static_assert(std::is_base_of_v<Session::Base, T>, "T must inherit from Session::Base");

            auto ip   = address.getIp();
            auto port = address.getPort();
            if (!ip || !port) return nullptr;
            const std::string target = std::format("{}:{}", ip.value(), port.value());

            std::unique_lock lock(mutex_);
            for (const auto& [id, session] : sessions_) {
                if (session->getAddressStr() == target)
                    return static_cast<T*>(session.get());
            }

            auto session = std::make_unique<T>(std::move(address), Common::nextId());
            T* raw = session.get();
            sessions_.emplace(raw->id(), std::move(session));
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
            for (const auto& [_, session] : sessions_)
                fn(session.get());
        }
        /**
         * @brief Iterates over sessions of a specific concrete type.
         *
         * Safely skips sessions that are not of type T.
         *
         * @tparam T   Concrete session type (TcpSession, UdpSession, etc.)
         *             Must inherit from Session::Base.
         * @param  fn  Callable receiving T*
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
                if (auto* ptr = dynamic_cast<T*>(session.get()))
                    fn(ptr);
        }
        SessionsInfoList getSessionsInfo() const {
            std::shared_lock lock(mutex_);
            SessionsInfoList data;
            data.reserve(sessions_.size());
            for (const auto& [id, session] : sessions_) {
                const auto&  regInfo = session->getRegistrationInfo();
                data.emplace_back(
                    BriefSessionInfo{
                        id,
                        session->type(),
                        session->status(),
                        session->idleSeconds(),
                        session->getAddressStr(),
                        regInfo.username,
                        regInfo.username,
                        regInfo.os,
                        regInfo.timezone

                    });
            };
            return data;
        }



    private:

        /// Owns all sessions — key is session id, value is the session itself.
        std::unordered_map<uint64_t, std::unique_ptr<Session::Base>> sessions_;

        mutable std::shared_mutex mutex_;
        std::thread thread_;
        std::atomic<bool> running_{false};

        std::condition_variable_any cv_;

        void startMonitor() {
            running_ = true;
            thread_  = std::thread([this] { monitorLoop(); });
        }
        void stopMonitor() {
            running_ = false;
            cv_.notify_one();
            if (thread_.joinable())
                thread_.join();
        }



        void monitorLoop() {
            while (running_) {
                std::vector<uint64_t> toRemove;
                {
                    std::shared_lock lock(mutex_);
                    for (const auto& [id, session] : sessions_) {
                        const auto idle = session->idleSeconds();

                        if (session->isDisconnected()) {
                            toRemove.push_back(id);
                            continue;
                        }

                        if (idle >= Config::TIMEOUT_SECONDS) {
                            session->setStatus(Session::Status::Disconnected);
                            toRemove.push_back(id);
                            continue;
                        }

                        if (idle >= Config::IDLE_SECONDS && session->isConnected())
                            session->setStatus(Session::Status::Idle);
                    }
                }

                if (!toRemove.empty()) {
                    std::unique_lock lock(mutex_);
                    for (const auto id : toRemove)
                        sessions_.erase(id);
                }

                std::shared_lock lock(mutex_);
                cv_.wait_for(lock, std::chrono::minutes(1), [this] { return !running_; });
            }
        }
    };

} // namespace Raptor::Core::Server
