#pragma once

#include "core/session/queue.hpp"
#include "libs/net/include/types.hpp"
#include "register.hpp"
#include "type.hpp"
#include <string>

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
        Base(Base&&)= delete;
        Base& operator=(Base&&) = delete;

        virtual ~Base() noexcept = default;


        /**
         * @brief Constructs a session with a unique id and protocol type.
         *
         * Both timestamps are set to now — idle and uptime both start at zero.
         *
         * @param id    Unique session id from Common::nextId().
         * @param type  Protocol type (TCP, UDP, etc.).
         */
        Base(const uint64_t id, Common::Types::ServerType type,const std::string& conn)
            : id_(id)
            , type_(type)
            , status_(Status::Connected)
            , connectedAt_(Common::Types::Clock::now())
            , lastActive_(Common::Types::Clock::now())
            , connectedTo_(conn)
        {}

        /**
         * @brief Reads data from the underlying socket into buf.
         *
         * Default implementation is a no-op — override in concrete session types.
         *
         * @param buf  Destination buffer.
         * @param len  Maximum bytes to read.
         * @return     Bytes read, or an error.
         */
        virtual Net::Result<size_t> read(void* buf, size_t len) noexcept
            { return Net::Result<size_t>(0); }

        /**
         * @brief Writes data from buf to the underlying socket.
         *
         * Default implementation is a no-op — override in concrete session types.
         *
         * @param buf  Source buffer.
         * @param len  Number of bytes to write.
         * @return     Bytes written, or an error.
         */
        virtual Net::Result<size_t> write(const void* buf, size_t len) noexcept
            { return Net::Result<size_t>(0); }

        /**
         * @brief Returns the remote address of this session as (ip, port).
         *
         * Pure virtual — every concrete session must implement this.
         *
         * @return  ip/port pair, or an error if the address is unavailable.
         */
        virtual std::string
            getAddressStr() const noexcept = 0;


        /**
         * @brief Returns the unique session id.
         *
         * Assigned at construction and never changes.
         */
        const uint64_t& id() const noexcept { return id_; }

        /**
         * @brief Returns the protocol type of this session (TCP, UDP, etc.).
         */
        const Common::Types::ServerType type() const noexcept { return type_; }

        /**
         * @brief Returns the current lifecycle status.
         *
         * @see Status
         */
        Status status() const noexcept { return status_; }

        /**
         * @brief Transitions the session to a new lifecycle status.
         *
         * @param s  New status to set.
         */
        void setStatus(Status s) noexcept { status_ = s; }

        const void setRegistrationInfo(const Common::Register& r) noexcept {
               information = r;
            }

        /** @brief Returns true if the session is actively connected. */
        bool isConnected()     const noexcept { return status_ == Status::Connected;     }

        /** @brief Returns true if the session has been idle for a while. */
        bool isIdle()          const noexcept { return status_ == Status::Idle;          }

        /** @brief Returns true if a graceful shutdown is in progress. */
        bool isDisconnecting() const noexcept { return status_ == Status::Disconnecting; }

        /** @brief Returns true if the socket is closed and the session is pending removal. */
        bool isDisconnected()  const noexcept { return status_ == Status::Disconnected;  }

        /** @brief Returns the timestamp when this session was created. */
        Common::Types::TimePoint connectedAt() const noexcept { return connectedAt_; }

        /** @brief Returns the timestamp of the last received data. */
        Common::Types::TimePoint lastActive()  const noexcept { return lastActive_;  }

        /**
         * @brief Resets the idle timer to now.
         *
         * Call this on every successful read to keep the session alive.
         * Also transitions Idle → Connected automatically, since receiving
         * data proves the client is still alive.
         */
        void touch() noexcept {
            lastActive_ = Common::Types::Clock::now();
            if (status_ == Status::Idle)
                setStatus(Status::Connected);
        }

        /**
         * @brief Returns seconds elapsed since the last received data.
         *
         * Used by SessionManager monitor to detect idle and timed-out sessions.
         * Resets to zero on every call to touch().
         *
         * @return  Idle duration in whole seconds.
         */
        uint64_t idleSeconds() const noexcept {
            return std::chrono::duration_cast<std::chrono::seconds>(
                Common::Types::Clock::now() - lastActive_).count();
        }

        /**
         * @brief Returns seconds elapsed since the session was created.
         *
         * Useful for logging and metrics. Never resets.
         *
         * @return  Uptime duration in whole seconds.
         */
        uint64_t uptimeSeconds() const noexcept {
            return std::chrono::duration_cast<std::chrono::seconds>(
                Common::Types::Clock::now() - connectedAt_).count();
        }

        /**
         * @brief Prints a one-line summary of this session to stdout.
         *
         * Includes id, protocol type, status, idle time, and uptime.
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
        const std::string& connectedTo() const noexcept {
            return connectedTo_;
        }
        /**
         * @brief Returns true if there is unsent data waiting in the send queue.
         *
         * Useful for checking whether it is safe to close the session
         * without dropping pending writes.
         */
        bool hasPendingWrites() const noexcept {
            return !sendQueue_.empty();
        }
        const Common::Register& getRegistrationInfo() const noexcept {
            return information;
        }
        private:
        uint64_t                      id_;
        Common::Types::ServerType     type_;
        Status   status_;
        Common::Types::TimePoint      connectedAt_;  ///< Set once at construction, never changes.
        Common::Types::TimePoint      lastActive_;   ///< Updated on every successful read via touch().
        Queue::SendQueue<std::string> sendQueue_;
        Common::Register information;
        std::string connectedTo_;
    };

} // namespace Raptor::Core::Session
