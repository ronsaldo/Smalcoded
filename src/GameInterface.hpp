#ifndef SMALL_ECO_DESTROYED_GAME_INTERFACE_HPP
#define SMALL_ECO_DESTROYED_GAME_INTERFACE_HPP

#include "MemoryZone.hpp"
#include "ControllerState.hpp"
#include "Framebuffer.hpp"

struct GameInterface
{
    virtual void setPersistentMemory(MemoryZone *zone) = 0;
    virtual void setTransientMemory(MemoryZone *zone) = 0;

    virtual void update(float delta, const ControllerState &controllerState) = 0;
    virtual void render(const Framebuffer &framebuffer) = 0;
};

typedef GameInterface *(*GetGameInterfaceFunction)();

#endif //SMALL_ECO_DESTROYED_GAME_INTERFACE_HPP
