#ifndef SMALL_ECO_DESTROYED_GAME_INTERFACE_HPP
#define SMALL_ECO_DESTROYED_GAME_INTERFACE_HPP

#include "MemoryZone.hpp"
#include "ControllerState.hpp"
#include "Framebuffer.hpp"

static constexpr size_t PersistentMemorySize = 8*1024*1024;
static constexpr size_t TransientMemorySize = 16*1024;//32*1024*1024;

struct GameInterface
{
    virtual void setPersistentMemory(MemoryZone *zone) = 0;
    virtual void setTransientMemory(MemoryZone *zone) = 0;

    virtual void update(float delta, const ControllerState &controllerState) = 0;
    virtual void render(const Framebuffer &framebuffer) = 0;
};

typedef GameInterface *(*GetGameInterfaceFunction)();

#endif //SMALL_ECO_DESTROYED_GAME_INTERFACE_HPP
