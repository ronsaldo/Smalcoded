#ifndef SMALL_ECO_DESTROYED_GAME_LOGIC_INTERFACE_HPP
#define SMALL_ECO_DESTROYED_GAME_LOGIC_INTERFACE_HPP

#include "MemoryZone.hpp"

struct GlobalState
{
};

extern GlobalState *globalState;

uint8_t *allocateTransientBytes(size_t byteCount);

template<typename T>
T *newTransient()
{
    return allocateTransientBytes(sizeof(T));
}

#endif //SMALL_ECO_DESTROYED_GAME_LOGIC_INTERFACE_HPP
