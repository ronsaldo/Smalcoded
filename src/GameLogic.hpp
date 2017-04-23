#ifndef SMALL_ECO_DESTROYED_GAME_LOGIC_INTERFACE_HPP
#define SMALL_ECO_DESTROYED_GAME_LOGIC_INTERFACE_HPP

#include "MemoryZone.hpp"
#include "ControllerState.hpp"
#include "Vector2.hpp"
#include "Box2.hpp"
#include "Tile.hpp"
#include "Random.hpp"

enum class SpriteType
{
    None = 0,
    Tile,
    Character,
};

struct AnimationState
{
    // Animation description
    int row;
    int column;
    int frameCount;
    float fps;
    bool looped;

    // Animation state.
    float playRate;
    float position;
    bool animating;

    bool isSameAs(const AnimationState &o) const
    {
        return row == o.row &&
            column == o.column &&
            frameCount == o.frameCount &&
            fps == o.fps &&
            looped == o.looped;
    }
};

enum class FaceOrientation
{
    Down = 0,
    Left,
    Up,
    Right
};

struct Entity
{
    Vector2 position;
    Vector2 velocity;
    Box2 boundingBox;
    Box2 feetBoundingBox;
    Box2 collisionBoundingBox;
    uint32_t tileMovementMask;

    SpriteType spriteType;
    int spriteRow;
    int spriteColumn;
    bool flipHorizontal;
    bool flipVertical;
    AnimationState animationState;
    FaceOrientation faceOrientation;

    void changeAnimation(const AnimationState &a)
    {
        if(!animationState.isSameAs(a))
            animationState = a;
    }
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
    Random random;

    CameraState camera;
    PlayerState player;
    TileMap map;
    TileSet mapTileSet;
    TileSet characterTileSet;
    MiniMapImage minimap;
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
