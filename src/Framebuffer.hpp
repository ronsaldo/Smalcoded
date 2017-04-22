#ifndef SMALL_ECO_DESTROYED_FRAMEBUFFER_HPP
#define SMALL_ECO_DESTROYED_FRAMEBUFFER_HPP

#include <stdint.h>

struct Framebuffer
{
    int width;
    int height;
    int pitch;
    uint8_t *pixels;
};

#endif //SMALL_ECO_DESTROYED_FRAMEBUFFER_HPP
