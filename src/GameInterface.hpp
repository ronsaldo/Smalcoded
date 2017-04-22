#ifndef SMALL_ECO_DESTROYED_GAME_INTERFACE_HPP
#define SMALL_ECO_DESTROYED_GAME_INTERFACE_HPP

#include "MemoryZone.hpp"

struct GameInterface
{
    virtual void setPersistentMemory(MemoryZone *zone) = 0;
    virtual void setTransientMemory(MemoryZone *zone) = 0;

    virtual void update(float delta) = 0;
    virtual void render(int width, int height, uint8_t *framebuffer, int pitch) = 0;
};

typedef GameInterface *(*GetGameInterfaceFunction)();

#endif //SMALL_ECO_DESTROYED_GAME_INTERFACE_HPP
