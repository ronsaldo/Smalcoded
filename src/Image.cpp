#include "Image.hpp"
#include "SDL_image.h"

void Image::load(const char *fileName)
{
    auto surface = IMG_Load(fileName);
    if(!surface)
        fprintf(stderr, "Failed to load image %s: %s\n", fileName, IMG_GetError());

    auto expectedSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(surface);

    width = expectedSurface->w;
    height = expectedSurface->h;
    pitch = expectedSurface->pitch;
    bpp = expectedSurface->format->BitsPerPixel;
    data = new uint8_t[pitch*height];
    memcpy(data, expectedSurface->pixels, pitch*height);
    SDL_FreeSurface(expectedSurface);
}

void Image::destroy()
{
    delete [] data;
}
