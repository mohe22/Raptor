#pragma once


#include <random>
#include "common/type.hpp"
namespace Raptor::Common{
    inline uint64_t nextId() noexcept {
        static std::mt19937_64 rng{std::random_device{}()};
        static std::uniform_int_distribution<uint64_t> dist;
        return dist(rng);
    }
    /**
     * @brief Converts a time point to elapsed seconds since now.
     *
     * Guards against negative durations caused by clock skew
     *
     * @param from  The starting time point to measure from.
     * @return Elapsed whole seconds, or 0 if clock went backward.
     */
    inline uint64_t toSeconds(const Common::Types::TimePoint& from) noexcept {
        auto dur = Common::Types::Clock::now() - from;
        auto s = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
        return s > 0 ? static_cast<uint64_t>(s) : 0;
    }
};
