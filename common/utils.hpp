#pragma once


#include <random>
namespace Raptor::Common{
    inline uint64_t nextId() noexcept {
        static std::mt19937_64 rng{std::random_device{}()};
        static std::uniform_int_distribution<uint64_t> dist;
        return dist(rng);
    }

};
