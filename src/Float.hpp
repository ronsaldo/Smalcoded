#ifndef SMALL_ECO_DESTROYED_FLOAT_HPP
#define SMALL_ECO_DESTROYED_FLOAT_HPP

#include <math.h>

static constexpr float FloatEpsilon = 0.0001f;

inline bool closeTo(float a, float b)
{
    auto d = a - b;
    return -FloatEpsilon <= d && d <= FloatEpsilon;
}

#endif //SMALL_ECO_DESTROYED_FLOAT_HPP
