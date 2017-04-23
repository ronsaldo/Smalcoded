#ifndef SMALL_ECO_DESTROYED_VECTOR2_HPP
#define SMALL_ECO_DESTROYED_VECTOR2_HPP

#include "Float.hpp"

class Vector2
{
public:
    constexpr Vector2(float cx=0.0f, float cy=0.0f)
        : x(cx), y(cy) {}

    float cross(const Vector2 &v) const
    {
        return x*v.y - y*v.x;
    }

    float dot(const Vector2 &v) const
    {
        return x*v.x + y*v.y;
    }

    float length2() const
    {
        return x*x + y*y;
    }

    float length() const
    {
        return sqrt(x*x + y*y);
    }

    Vector2 normalized() const
    {
        auto l = length();
        if(closeTo(l, 0))
            return Vector2();

        return Vector2(x/l, y/l);
    }

    Vector2 floor() const
    {
        return Vector2(::floor(x), ::floor(y));
    }

    Vector2 ceil() const
    {
        return Vector2(::ceil(x), ::ceil(y));
    }

    friend Vector2 operator-(const Vector2 &v)
    {
        return Vector2(-v.x, -v.y);
    }

    friend Vector2 operator+(const Vector2 &a, const Vector2 &b)
    {
        return Vector2(a.x + b.x, a.y + b.y);
    }

    friend Vector2 operator-(const Vector2 &a, const Vector2 &b)
    {
        return Vector2(a.x - b.x, a.y - b.y);
    }

    friend Vector2 operator*(const Vector2 &a, float s)
    {
        return Vector2(a.x * s, a.y * s);
    }

    friend Vector2 operator*(float s, const Vector2 &a)
    {
        return Vector2(s * a.x, s * a.y);
    }

    friend Vector2 operator*(const Vector2 &a, const Vector2 &b)
    {
        return Vector2(a.x * b.x, a.y * b.y);
    }

    Vector2 &operator+=(const Vector2 &o)
    {
        x += o.x;
        y += o.y;
        return *this;
    }

    Vector2 &operator-=(const Vector2 &o)
    {
        x -= o.x;
        y -= o.y;
        return *this;
    }

    Vector2 &operator*=(const Vector2 &o)
    {
        x *= o.x;
        y *= o.y;
        return *this;
    }

    Vector2 &operator*=(float s)
    {
        x *= s;
        y *= s;
        return *this;
    }

    bool isNotZero() const
    {
        return x != 0 || y != 0;
    }
    float x, y;
};

inline Vector2 units2Pixels(const Vector2 &v)
{
    return v*Units2Pixels;
}

inline Vector2 pixels2Units(const Vector2 &v)
{
    return v*Pixels2Units;
}

inline int orient2D(const Vector2 &e1, const Vector2 &e2, const Vector2 &p)
{
    return signOrZero((e2 - e1).cross(p - e1));
}

#endif //SMALL_ECO_DESTROYED_VECTOR2_HPP
