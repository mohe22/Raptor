#pragma once

#include "core/session/queue.hpp"
#include "libs/net/include/types.hpp"
#include <utility>
#include "type.hpp"

namespace Raptor::Core::Session {



    /**
     * @brief Lifecycle status of a session.
     *
     *  Connected → Idle          (no data for N seconds)
     *  Connected → Disconnecting (graceful shutdown in progress)
     *  Any       → Disconnected  (socket closed, pending removal)
     */
    enum class Status : uint8_t {
        Connected,      ///< Active — data flowing normally
        Idle,           ///< No data received for a while — may be timed out soon
        Disconnecting,  ///< Graceful shutdown initiated — draining writes
        Disconnected,   ///< Socket closed — session should be removed
    };

    static const char* ToString(Status s) noexcept {
        switch (s) {
            case Status::Connected:     return "Connected";
            case Status::Idle:          return "Idle";
            case Status::Disconnecting: return "Disconnecting";
            case Status::Disconnected:  return "Disconnected";
            default:                    return "Unknown";
        }
    }

      class Base {
    public:
        Base(const Base&)            = delete;
        Base& operator=(const Base&) = delete;
        Base(Base&&)                 = delete;
        Base& operator=(Base&&)      = delete;

        virtual ~Base() noexcept = default;

        Base(const uint64_t id, Common::Types::ServerType type)
            : id_(id)
            , type_(type)
            , status_(Status::Connected)
            , connectedAt_(Common::Types::Clock::now())
            , lastActive_(Common::Types::Clock::now())
        {}



        virtual Net::Result<size_t> read(void* buf, size_t len) noexcept
            { return Net::Result<size_t>(0); }
        virtual Net::Result<size_t> write(const void* buf, size_t len) noexcept
            { return Net::Result<size_t>(0); }
        virtual Net::Result<std::pair<std::string_view, uint16_t>>
            getAddressStr() const noexcept = 0;


        uint64_t id()   const noexcept { return id_;   }
        const Common::Types::ServerType     type() const noexcept { return type_; }


        /// Current lifecycle status.
        Status status() const noexcept { return status_; }
        /// Transition to a new status.
        void setStatus(Status s) noexcept { status_ = s; }

        bool isConnected()     const noexcept { return status_ == Status::Connected;     }
        bool isIdle()          const noexcept { return status_ == Status::Idle;          }
        bool isDisconnecting() const noexcept { return status_ == Status::Disconnecting; }
        bool isDisconnected()  const noexcept { return status_ == Status::Disconnected;  }


        /// When the session was constructed (connection accepted).
        Common::Types::TimePoint connectedAt() const noexcept { return connectedAt_; }

        /// When data was last received from this session.
        Common::Types::TimePoint lastActive()  const noexcept { return lastActive_;  }

        /**
         * @brief Mark session as active — call on every successful read.
         *
         * Also transitions Idle → Connected automatically,
         * since receiving data proves the client is still alive.
         */
        void touch() noexcept {
            lastActive_ = Common::Types::Clock::now();
            if (status_ == Status::Idle)
                setStatus(Status::Connected);
        }

        /**
         * @brief Seconds since the last received data.
         *
         * Used by SessionManager monitor to detect idle/timed-out sessions.
         */
        uint64_t idleSeconds() const noexcept {
            return std::chrono::duration_cast<std::chrono::seconds>(
                Common::Types::Clock::now() - lastActive_).count();
        }

        /**
         * @brief Seconds since the session connected.
         *
         * Useful for logging and metrics.
         */
        uint64_t uptimeSeconds() const noexcept {
            return std::chrono::duration_cast<std::chrono::seconds>(
                Common::Types::Clock::now() - connectedAt_).count();
        }

        /**
         * @brief Prints a one-line summary of this session's current state.
         *
         * @code
         *   manager.forEach([](Session::Base* s) { s->print(); });
         * @endcode
         */
        void print() const noexcept {
            std::println("Session {{ id={} type={} status={} idle={}s uptime={}s }}",
                id_,
                ToString(type_),
                ToString(status_),
                idleSeconds(),
                uptimeSeconds()
            );
        }
        bool hasPendingWrites() const noexcept {
            return !sendQueue_.empty();
        }

    private:
        uint64_t   id_;
        Common::Types::ServerType   type_;
        Status     status_;
        Common::Types::TimePoint  connectedAt_;  ///< Set once at construction, never changes.
        Common::Types::TimePoint  lastActive_;   ///< Updated on every successful read via touch().
        Queue::SendQueue<std::string> sendQueue_;
    };

} // namespace Raptor::Core::Session
