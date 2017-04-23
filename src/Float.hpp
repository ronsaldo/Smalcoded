#ifndef SMALL_ECO_DESTROYED_FLOAT_HPP
#define SMALL_ECO_DESTROYED_FLOAT_HPP

#include <math.h>

static constexpr float FloatEpsilon = 0.0001f;
static constexpr float Units2Pixels = 32.0f;
static constexpr float Pixels2Units = 1.0f/Units2Pixels;
static constexpr float KmHr2MS = 1.0/3.6f;

inline bool closeTo(float a, float b)
{
    auto d = a - b;
    return -FloatEpsilon <= d && d <= FloatEpsilon;
}

inline float units2Pixels(float v)
{
    return v*Units2Pixels;
}

inline float pixels2Units(float v)
{
    return v*Pixels2Units;
}

inline int floorModule(int x, int d)
{
    return x - floor(float(x)/float(d))*d;
}

inline float floorModule(float x, float d)
{
    return x - floor(x/d)*d;
}

#endif //SMALL_ECO_DESTROYED_FLOAT_HPP
