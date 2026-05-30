#pragma once

#include <atomic>
#include <cstdint>
namespace Raptor::Common{
    inline uint64_t nextId() noexcept {
        static std::atomic<uint64_t> counter{1};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }
};
