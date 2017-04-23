#ifndef SMALL_ECO_DESTROYED_GAME_LOGIC_INTERFACE_HPP
#define SMALL_ECO_DESTROYED_GAME_LOGIC_INTERFACE_HPP

#include "GameInterface.hpp"
#include "ControllerState.hpp"
#include "Vector2.hpp"
#include "Box2.hpp"
#include "Tile.hpp"
#include "Random.hpp"
#include <algorithm>

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

    float health;

    void changeAnimation(const AnimationState &a)
    {
        if(!animationState.isSameAs(a))
            animationState = a;
    }

    int roundedHealth() const
    {
        return int(health + 0.5f);
    }

    void receiveDamage(float damage)
    {
        health = std::max(health - damage, 0.0f);
    }

    bool isAlive()
    {
        return roundedHealth() > 0;
    }

    Vector2 faceOrientationVector() const
    {
        switch(faceOrientation)
        {
        default:
        case FaceOrientation::Down: return Vector2(0, -1);
        case FaceOrientation::Left: return Vector2(-1, 0);
        case FaceOrientation::Up: return Vector2(0, 1);
        case FaceOrientation::Right: return Vector2(1, 0);
        }
    }
};

struct PlayerState : Entity
{
    float belly;
    bool running;
    int bullets;
    int demolitionBullets;
    bool withDemolitionBullets;

    int roundedBelly() const
    {
        return int(belly + 0.5f);
    }

    bool isActuallyRunning() const
    {
        return running && velocity.isNotZero();
    }

    void increaseBelly(float amount)
    {
        belly = std::min(belly + amount, 100.0f);
    }

    void increaseHealth(float amount)
    {
        health = std::min(health + amount, 100.0f);
    }

    Vector2 fireDirection() const
    {
        if(velocity.isNotZero())
            return velocity.normalized();
        return faceOrientationVector();
    }
};

struct CameraState
{
    Vector2 position;
};

static constexpr size_t MaxNumberOfBullets = 1024;

namespace BulletFlags
{
enum Flag
{
    None = 0,
    FiredByPlayer = 1<<0,
    Demolition = 1<<1,
    FiredByEnemy = 1<<2,
    FiredByTurret = 1<<3,
};
};

struct BulletState
{
    Vector2 position;
    Vector2 velocity;
    Box2 boundingBox;
    float timeToLive;
    float power;
    uint32_t color;
    uint32_t flashColor;
    uint32_t flags;

    bool isAlive() const
    {
        return timeToLive > 0.0f;
    }

    bool wasFiredByPlayer() const
    {
        return flags & BulletFlags::FiredByPlayer;
    }

    bool wasFiredByEnemy() const
    {
        return flags & BulletFlags::FiredByEnemy;
    }

    bool wasFiredByTurret() const
    {
        return flags & BulletFlags::FiredByTurret;
    }

    bool isDemolition() const
    {
        return flags & BulletFlags::Demolition;
    }

    void gotTarget()
    {
        // Mark as dead.
        timeToLive = -1;
    }
};

struct GlobalState
{
    // Assets
    TileMap map;
    TileSet mapTileSet;
    TileSet characterTileSet;
    TileSet spriteSet;
    MiniMapImage minimap;

    // Global states
    bool isInitialized;
    bool isPaused;
    float currentTime;
    float matchTime;
    ControllerState oldControllerState;
    ControllerState controllerState;
    Random random;

    // Some "entities"
    CameraState camera;
    PlayerState player;
    BulletState bullets[MaxNumberOfBullets];

    int numberOfDeadBullets;
    int deadBullets[MaxNumberOfBullets];
    int numberOfAliveBullets;
    int aliveBullets[MaxNumberOfBullets];

    bool isButtonPressed(int button) const
    {
        return controllerState.getButton(button) && !oldControllerState.getButton(button);
    }
};

static_assert(sizeof(GlobalState) < PersistentMemorySize, "Increase the persistentMemory");

extern GlobalState *globalState;

#define global (*globalState)

uint8_t *allocateTransientBytes(size_t byteCount);

template<typename T>
T *newTransient()
{
    return allocateTransientBytes(sizeof(T));
}

#endif //SMALL_ECO_DESTROYED_GAME_LOGIC_INTERFACE_HPP
