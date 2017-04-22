#include "GameInterface.hpp"
#include "GameLogic.hpp"
#include "Renderer.hpp"

GlobalState *globalState;
static MemoryZone *transientMemoryZone;

uint8_t *allocateTransientBytes(size_t byteCount)
{
    return transientMemoryZone->allocateBytes(byteCount);
}

void update(float delta)
{

}

class GameInterfaceImpl : public GameInterface
{
public:
    virtual void setPersistentMemory(MemoryZone *zone) override;
    virtual void setTransientMemory(MemoryZone *zone) override;
    virtual void update(float delta) override;
    virtual void render(int width, int height, uint8_t *framebuffer, int pitch) override;
};

void GameInterfaceImpl::setPersistentMemory(MemoryZone *zone)
{
    globalState = reinterpret_cast<GlobalState*> (zone->getData());
}

void GameInterfaceImpl::setTransientMemory(MemoryZone *zone)
{
    transientMemoryZone = zone;
}

void GameInterfaceImpl::update(float delta)
{
    ::update(delta);
}

void GameInterfaceImpl::render(int width, int height, uint8_t *framebuffer, int pitch)
{
    ::render(width, height, framebuffer, pitch);
}

static GameInterfaceImpl gameInterfaceImpl;

extern "C" GameInterface *getGameInterface()
{
    return &gameInterfaceImpl;
}
