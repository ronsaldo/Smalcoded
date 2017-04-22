#include "GameInterface.hpp"
#include "GameLogic.hpp"
#include "Renderer.hpp"

GlobalState *globalState;
static MemoryZone *transientMemoryZone;

uint8_t *allocateTransientBytes(size_t byteCount)
{
    return transientMemoryZone->allocateBytes(byteCount);
}

void initializeGlobalState()
{
    if(global.isInitialized)
        return;

    global.isInitialized = true;
}

void update(float delta, const ControllerState &controllerState)
{
    initializeGlobalState();

    // Store the current time and update the controller state.
    global.currentTime += delta;
    global.oldControllerState = global.controllerState;
    global.controllerState = controllerState;

    global.player.velocity = Vector2(controllerState.leftXAxis, controllerState.leftYAxis).normalized()*100;
    global.player.position += global.player.velocity*delta;

}

class GameInterfaceImpl : public GameInterface
{
public:
    virtual void setPersistentMemory(MemoryZone *zone) override;
    virtual void setTransientMemory(MemoryZone *zone) override;
    virtual void update(float delta, const ControllerState &controllerState) override;
    virtual void render(const Framebuffer &framebuffer) override;
};

void GameInterfaceImpl::setPersistentMemory(MemoryZone *zone)
{
    globalState = reinterpret_cast<GlobalState*> (zone->getData());
}

void GameInterfaceImpl::setTransientMemory(MemoryZone *zone)
{
    transientMemoryZone = zone;
}

void GameInterfaceImpl::update(float delta, const ControllerState &controllerState)
{
    ::update(delta, controllerState);
}

void GameInterfaceImpl::render(const Framebuffer &framebuffer)
{
    ::render(framebuffer);
}

static GameInterfaceImpl gameInterfaceImpl;

extern "C" GameInterface *getGameInterface()
{
    return &gameInterfaceImpl;
}
