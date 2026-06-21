#pragma once

#include "core/session/queue.hpp"
#include "core/session/task/task.hpp"
#include "libs/net/include/types.hpp"
#include "register.hpp"
#include "type.hpp"
#include <chrono>
#include <ctime>
#include <memory>
#include <string>

namespace Raptor::Core::Session {

    /**
     * @brief Lifecycle status of a session.
     *  Connected -> Idle          (no data for N seconds)
     *  Connected -> Disconnecting (graceful shutdown in progress)
     *  Any       -> Disconnected  (socket closed, pending removal)
     */
    enum class Status : uint8_t {
        Connected,     ///< Active — data flowing normally
        Idle,          ///< No data received for a while
        Disconnecting, ///< Graceful shutdown initiated
        Disconnected,  ///< Socket closed — pending removal
    };

    inline const char* ToString(Status s) noexcept {
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
        Base(const Base&) = delete;
        Base& operator=(const Base&) = delete;
        Base(Base&&) = delete;
        Base& operator=(Base&&) = delete;
        virtual ~Base() noexcept = default;

        Base(const uint64_t id, Common::Types::ServerType type,const std::string&conneted)
            : id_(id)
            , type_(type)
            , status_(Status::Connected)
            , connectedAt_(Common::Types::Clock::now())          ///< Monotonic  duration math.
            , connectedAtWall_(std::chrono::system_clock::now()) ///< Wall clock  serialization.
            , lastActive_(Common::Types::Clock::now())
            , connectedTo_(conneted)
        {}

        virtual Net::Result<size_t> read(void*, size_t) noexcept  { return Net::Result<size_t>(0); }
        virtual Net::Result<size_t> write(const void*, size_t) noexcept { return Net::Result<size_t>(0); }
        virtual std::string getAddressStr() const noexcept = 0;

        const uint64_t& id() const noexcept { return id_;}
        Common::Types::ServerType type()const noexcept { return type_;  }
        Status  status() const noexcept { return status_; }
        const std::string& connectedTo() const noexcept {return connectedTo_;};
        void setStatus(Status s) noexcept { status_ = s; }
        void setRegistrationInfo(std::unique_ptr<Common::Register>&& r) noexcept {
            information = std::move(r);
        }
        const Common::Register& getRegistrationInfo() const noexcept {
            return *information;
        }

        bool isConnected() const noexcept { return status_ == Status::Connected;}
        bool isIdle() const noexcept { return status_ == Status::Idle;}

        bool isDisconnecting() const noexcept { return status_ == Status::Disconnecting; }
        bool isDisconnected() const noexcept { return status_ == Status::Disconnected;  }

        /// Seconds elapsed since the last received data. Resets to 0 on touch().
        [[nodiscard]] uint64_t idleSeconds() const noexcept {
            return static_cast<uint64_t>(
                 std::chrono::duration_cast<std::chrono::seconds>(
                     Common::Types::Clock::now() - lastActive_
                 ).count()
             );
        }

        /// Seconds elapsed since the session was created.
        [[nodiscard]] uint64_t uptimeSeconds() const noexcept {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    Common::Types::Clock::now() - connectedAt_
                ).count()
            );

        }

        /// Unix timestamp (seconds since epoch) — use for storage and SessionDetails.
        [[nodiscard]] uint64_t connectedAtUnix() const noexcept {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    connectedAtWall_.time_since_epoch()
                ).count()
            );
        }

        /// ISO 8601 string — use for logging and UI serialization.
        [[nodiscard]] std::string connectedAtStr() const noexcept {
            const std::time_t t = std::chrono::system_clock::to_time_t(connectedAtWall_);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
            return buf;
        }

        /// Resets the idle timer and transitions Idle Connected if needed.
        void touch() noexcept {
            lastActive_ = Common::Types::Clock::now();
            if (status_ == Status::Idle)
                setStatus(Status::Connected);
        }

        bool hasPendingWrites() const noexcept {
            return !sendQ_.empty();
        }

        /// Queue a command for sending to the client.
        void sendCommand(std::string cmd, const Common::PacketId& id,const Tasks::TaskPriority prio) noexcept {
            sendQ_.push(Tasks::makeCommand(std::move(cmd), id,prio));
        }
    protected:
        Queue::SendQueue<Tasks::Task> sendQ_;

    private:
        uint64_t   id_;
        Common::Types::ServerType type_;
        Status status_;
        Common::Types::TimePoint connectedAt_;     ///< Monotonic  duration math only.
        std::chrono::system_clock::time_point connectedAtWall_; ///< Wall clock  serialization / UI.
        Common::Types::TimePoint lastActive_;      ///< Reset on every touch().
        std::unique_ptr<Common::Register> information;
        std::string connectedTo_;
    };

} // namespace Raptor::Core::Session
