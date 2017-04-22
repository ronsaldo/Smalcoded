#ifndef SMALL_ECO_DESTROYED_RENDERER_HPP
#define SMALL_ECO_DESTROYED_RENDERER_HPP

#include <stdint.h>

inline uint32_t encodeBGRA(int r, int g, int b, int a)
{
    return b | (g<<8) | (r<<16) | (a<<24);
}

inline void decodeBGRA(uint32_t color, int &r, int &g, int &b, int &a)
{
    b = color & 0xFF;
    g = (color >> 8) & 0xFF;
    r = (color >> 16) & 0xFF;
    a = (color >> 24) & 0xFF;
}

void render(int width, int height, uint8_t *framebuffer, int pitch);

#endif //SMALL_ECO_DESTROYED_RENDERER_HPP
