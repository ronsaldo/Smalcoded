#ifndef SMALL_ECO_DESTROYED_GAME_LOGIC_INTERFACE_HPP
#define SMALL_ECO_DESTROYED_GAME_LOGIC_INTERFACE_HPP

#include "MemoryZone.hpp"
#include "ControllerState.hpp"
#include "Vector2.hpp"

struct Entity
{
    Vector2 position;
    Vector2 velocity;
};

struct PlayerState : Entity
{
};

struct CameraState
{
    Vector2 position;
};

struct GlobalState
{
    bool isInitialized;
    float currentTime;
    ControllerState oldControllerState;
    ControllerState controllerState;

    CameraState camera;
    PlayerState player;
};

extern GlobalState *globalState;

#define global (*globalState)

uint8_t *allocateTransientBytes(size_t byteCount);

template<typename T>
T *newTransient()
{
    return allocateTransientBytes(sizeof(T));
}

#endif //SMALL_ECO_DESTROYED_GAME_LOGIC_INTERFACE_HPP
