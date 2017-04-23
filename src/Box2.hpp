#ifndef SMALL_ECO_DESTROYED_BOX2_HPP
#define SMALL_ECO_DESTROYED_BOX2_HPP

#include "Vector2.hpp"

class Box2
{
public:
    Box2(const Vector2 &min, const Vector2 &max)
        : min(min), max(max) {}
    Box2(float nx=INFINITY, float ny=INFINITY, float px=-INFINITY, float py=-INFINITY)
        : min(nx, ny), max(px, py) {}

    static Box2 fromCenterAndHalfExtent(const Vector2 &center, const Vector2 &halfExtent)
    {
        return Box2(center - halfExtent, center + halfExtent);
    }

    static Box2 fromCenterAndExtent(const Vector2 &center, const Vector2 &extent)
    {
        return fromCenterAndHalfExtent(center, extent*0.5);
    }

    Box2 translatedBy(const Vector2 &v) const
    {
        return Box2(min + v, max + v);
    }

    float width() const
    {
        return max.x - min.x;
    }

    float height() const
    {
        return max.y - min.y;
    }

    Vector2 extent() const
    {
        return max - min;
    }

    Vector2 halfExtent() const
    {
        return (max - min) * 0.5f;
    }

    Vector2 bottomLeft() const
    {
        return min;
    }

    Vector2 bottomRight() const
    {
        return Vector2(max.x, min.y);
    }

    Vector2 topLeft() const
    {
        return Vector2(min.x, max.y);
    }

    Vector2 topRight() const
    {
        return max;
    }

    Vector2 center() const
    {
        return (min + max)*0.5;
    }

    Box2 shinkBy(const Vector2 &amount)
    {
        return fromCenterAndExtent(center(), extent() - amount);
    }

    Vector2 min, max;
};

inline Box2 units2Pixels(const Box2 &b)
{
    return Box2(units2Pixels(b.min), units2Pixels(b.max));
}

#endif //SMALL_ECO_DESTROYED_BOX2_HPP
