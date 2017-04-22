#include "Renderer.hpp"
#include "GameLogic.hpp"

void render(int width, int height, uint8_t *framebuffer, int pitch)
{
    auto rowStart = framebuffer;
    for(int y = 0; y < height; ++y, rowStart += pitch)
    {
        auto row = reinterpret_cast<uint32_t*> (rowStart);
        for(int x = 0; x < width; ++x)
        {
            int r = x & 255;
            int g = y & 255;
            int b = 0;

            row[x] = encodeBGRA(r, g, b, 255);
        }
    }
}
