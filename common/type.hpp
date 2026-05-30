#pragma once
#include <chrono>
namespace Raptor::Common::Types{


    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

}
