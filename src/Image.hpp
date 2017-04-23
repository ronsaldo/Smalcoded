#ifndef SMALL_ECO_DESTROYED_IMAGE_HPP
#define SMALL_ECO_DESTROYED_IMAGE_HPP

#include <stdint.h>

class Image
{
public:
    Image()
        : width(0), height(0), pitch(0), bpp(0), data(nullptr)
    {}

    void load(const char *fileName);
    void destroy();

    int width;
    int height;
    int pitch;
    int bpp;
    uint8_t *data;
};


#endif //SMALL_ECO_DESTROYED_IMAGE_HPP
