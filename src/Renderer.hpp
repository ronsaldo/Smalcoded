#ifndef SMALL_ECO_DESTROYED_RENDERER_HPP
#define SMALL_ECO_DESTROYED_RENDERER_HPP

#include <stdint.h>
#include "Framebuffer.hpp"

inline uint32_t encodeColor(int r, int g, int b, int a)
{
    return r | (g<<8) | (b<<16) | (a<<24);
}

inline void decodeColor(uint32_t color, int &r, int &g, int &b, int &a)
{
    r = color & 0xFF;
    g = (color >> 8) & 0xFF;
    b = (color >> 16) & 0xFF;
    a = (color >> 24) & 0xFF;
}

inline uint32_t convertRGBAHex(uint32_t color)
{
    int r, g, b, a;
    decodeColor(color, r, g, b, a);
    return encodeColor(b, g, r, a);
}

void render(const Framebuffer &framebuffer);

#endif //SMALL_ECO_DESTROYED_RENDERER_HPP
