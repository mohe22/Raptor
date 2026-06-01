#pragma once
#include <chrono>
namespace Raptor::Common::Types{


    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    enum class ServerType { TCP, UDP, HTTP };
    inline const char* ToString(ServerType type) noexcept {
        switch (type) {
            case ServerType::TCP:   return "TCP";
            case ServerType::UDP:   return "UDP";
            case ServerType::HTTP:  return "HTTP";
            default:  return "Unknown";
        }
    }




}
