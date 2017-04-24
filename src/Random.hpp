#ifndef SMALL_ECO_DESTROYED_RANDOM_HPP
#define SMALL_ECO_DESTROYED_RANDOM_HPP

#include <stdint.h>
#include <stdio.h>

struct Random
{
    // https://en.wikipedia.org/wiki/Linear_congruential_generator . MMIX
    static constexpr uint64_t Multiplier = 6364136223846793005ull;
    static constexpr uint64_t Increment = 1442695040888963407ull;

    static uint64_t hash(uint64_t value)
    {
        return value*Multiplier + Increment;
    }

    static uint32_t hash32(uint32_t value)
    {
        return hash(value) >> 32;
    }

    static uint32_t hashBit(uint32_t value)
    {
        auto result = hash32(value);
        result = (result ^ (result >> 16)) & 0xFFFF;
        result = (result ^ (result >> 8)) & 0xFF;
        result = (result ^ (result >> 4)) & 0xF;
        result = (result ^ (result >> 2)) & 0x7;
        result = (result ^ (result >> 1)) & 0x3;
        return result & 1;
    }

    uint64_t next()
    {
        return seed = hash(seed);
    }

    uint32_t next32()
    {
        return next() >> 32;
    }

    float nextFloat()
    {
        return double(next()) / double(uint64_t(-1));
    }

    float nextFloatInRange(float min, float max)
    {
        return nextFloat()*(max - min) + min;
    }

    uint64_t seed;
};


#endif //SMALL_ECO_DESTROYED_RANDOM_HPP
