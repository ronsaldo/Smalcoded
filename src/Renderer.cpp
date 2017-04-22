#include "Renderer.hpp"
#include "GameLogic.hpp"
#include <math.h>
#include <algorithm>

inline int clampCoordinate(int min, int max, int x)
{
    if(x < min)
        return min;
    else if(x > max)
        return max;
    else
        return x;
}

static void drawRectangle(const Framebuffer &framebuffer, uint32_t color, int x, int y, int width, int height)
{
    auto minX = x;
    auto minY = y;
    auto maxX = x + width;
    auto maxY = y + height;

    minX = clampCoordinate(0, framebuffer.width, minX);
    maxX = clampCoordinate(0, framebuffer.width, maxX);
    minY = clampCoordinate(0, framebuffer.height, minY);
    maxY = clampCoordinate(0, framebuffer.height, maxY);

    auto rowStart = framebuffer.pixels + minY* framebuffer.pitch;
    for(int dy = minY; dy < maxY; ++dy, rowStart += framebuffer.pitch)
    {
        auto row = reinterpret_cast<uint32_t*> (rowStart);
        for(int dx = minX; dx < maxX; ++dx)
            row[dx] = color;
    }
}

static void renderBackground(const Framebuffer &framebuffer)
{
    auto translation = -global.camera.position;

    int xOffset = floor(translation.x) + framebuffer.width/2;
    int yOffset = floor(translation.y) + framebuffer.height/2;

    auto rowStart = framebuffer.pixels + (framebuffer.height - 1)* framebuffer.pitch;
    for(int dy = 0; dy < framebuffer.height; ++dy, rowStart -= framebuffer.pitch)
    {
        auto row = reinterpret_cast<uint32_t*> (rowStart);
        for(int dx = 0; dx < framebuffer.width; ++dx)
        {
            int x = dx + xOffset;
            int y = dy + yOffset;

            int r = x & 255;
            int g = y & 255;
            int b = 0;

            row[dx] = encodeColor(r, g, b, 255);
        }
    }
}

static void renderEntity(const Framebuffer &framebuffer, const Entity &entity)
{
    auto translation = entity.position - global.camera.position;

    int xOffset = floor(translation.x) + framebuffer.width/2;
    int yOffset = floor(-translation.y) + framebuffer.height/2;

    drawRectangle(framebuffer, encodeColor(255, 255, 255, 255), xOffset-32, yOffset-32, 64, 64);
}

static void renderEntities(const Framebuffer &framebuffer)
{
    renderEntity(framebuffer, global.player);
}

void render(const Framebuffer &framebuffer)
{
    renderBackground(framebuffer);
    renderEntities(framebuffer);
}
