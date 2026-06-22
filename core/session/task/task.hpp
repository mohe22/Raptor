#pragma once
#include "common/header.hpp"
#include <cstdint>
#include <string>
#include <variant>

namespace Raptor::Core::Tasks {

    template<typename... Ts>
    struct Overload : Ts... { using Ts::operator()...; };

    struct Command {
        std::string cmd;
    };

    using TaskPayload = std::variant<Command>;

    enum class TaskPriority : uint8_t {
        Low      = 0,
        Normal   = 1,
        High     = 2,
        Critical = 3
    };

    enum class TaskState : uint8_t {
        SendingHeader,      // not started
        SendingData,  // writing payload
        Done
    };

    struct Task {
        Common::PacketId packetId {};
        TaskPriority priority{TaskPriority::Normal};
        TaskState state{TaskState::SendingHeader};
        std::array<uint8_t, Common::Header::SIZE> headerBuffer;
        TaskPayload payload;
        size_t offset{0};

        Task() = default;
        Task(const Task&)= delete;
        Task& operator=(const Task&) =delete;
        Task(Task&&) = default;
        Task& operator=(Task&&) = default;

        bool operator<(const Task& o) const noexcept {
            return static_cast<uint8_t>(priority) < static_cast<uint8_t>(o.priority);
        }
    };

    inline Task makeTask(std::string cmd,const Common::PacketId& id,const Common::PacketType type,TaskPriority prio = TaskPriority::Normal) noexcept
    {
        Task t;
        t.packetId = id;
        t.priority = prio;

        Common::Header header;
        header.type =type;
        header.packetId = id;
        header.payloadSize = cmd.size();
        header.setFlag(Common::Flags::Text);

        t.headerBuffer = header.serialize();

        t.payload = Command{ .cmd = std::move(cmd) };

        return t;
    }

} // namespace Raptor::Core::Tasks
