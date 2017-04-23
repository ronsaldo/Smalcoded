#include "GameInterface.hpp"
#include "GameLogic.hpp"
#include "Renderer.hpp"
#include <algorithm>
#include <stdio.h>

GlobalState *globalState;
static MemoryZone *transientMemoryZone;

uint8_t *allocateTransientBytes(size_t byteCount)
{
    return transientMemoryZone->allocateBytes(byteCount);
}

static const AnimationState PlayerAnim_IdleDown = {0, 0, 2, 4, true, 1.0f};
static const AnimationState PlayerAnim_WalkDown = {0, 2, 4, 4, true, 1.0f};
static const AnimationState PlayerAnim_IdleRight = {0, 6, 2, 4, true, 1.0f};
static const AnimationState PlayerAnim_WalkRight = {0, 8, 4, 4, true, 1.0f};
static const AnimationState PlayerAnim_IdleUp = {0, 12, 2, 4, true, 1.0f};
static const AnimationState PlayerAnim_WalkUp = {1, 0, 4, 4, true, 1.0f};

void initializePlayer(PlayerState &player)
{
    player.spriteType = SpriteType::Character;
    player.animationState = PlayerAnim_IdleDown;

    player.position = Vector2(180, 130);
    player.boundingBox = Box2::fromCenterAndExtent(Vector2(), Vector2(1.0, 1.5));
    player.collisionBoundingBox = player.boundingBox;
    player.feetBoundingBox = Box2(player.boundingBox.min, player.boundingBox.min + Vector2(player.boundingBox.width(), player.boundingBox.height()/2))
                            .shinkBy(Vector2(0.1, 0.0));
    player.tileMovementMask = TileTypeMask::AnyGround | TileTypeMask::ShallowWater;
}

bool canEntityMoveToTileAtPoint(const Entity &entity, const Vector2 &targetPoint)
{
    auto tileIndex = global.map.tileIndexAtPoint(targetPoint);
    auto tileType = global.map.tiles[tileIndex];
    return isTileTypeInSet(tileType, entity.tileMovementMask);
}

bool canEntityMoveToPoint(const Entity &entity, const Vector2 &targetPoint)
{
    return
        canEntityMoveToTileAtPoint(entity, entity.feetBoundingBox.bottomLeft() + targetPoint) &&
        canEntityMoveToTileAtPoint(entity, entity.feetBoundingBox.bottomRight() + targetPoint) &&
        canEntityMoveToTileAtPoint(entity, entity.feetBoundingBox.topRight() + targetPoint) &&
        canEntityMoveToTileAtPoint(entity, entity.feetBoundingBox.topLeft() + targetPoint) &&
        canEntityMoveToTileAtPoint(entity, entity.feetBoundingBox.center() + targetPoint); // Is this needed?
}

void initializeGlobalState()
{
    if(global.isInitialized)
        return;

    global.map.loadFromFile("assets/earth_map.png");
    global.mapTileSet.loadFromFile("assets/tiles.png");
    global.characterTileSet.loadFromFile("assets/character-sprites.png");
    global.minimap.loadFromFile("assets/minimap.png");
    initializePlayer(global.player);

    global.isInitialized = true;
}

void updateEntityAnimation(float delta, Entity &entity)
{
    auto &state = entity.animationState;
    if(state.frameCount == 0)
        return;

    auto duration = state.frameCount/state.fps;
    state.position += delta*state.playRate;
    if(state.looped)
        state.position = floorModule(state.position, duration);
    else
        state.position = std::min(state.position, duration);

    int frame = floor(state.position * state.fps);
    entity.spriteRow = state.row;
    entity.spriteColumn = state.column + frame;
}

void updatePlayer(float delta, PlayerState &player)
{
    auto direction = Vector2(global.controllerState.leftXAxis, global.controllerState.leftYAxis).normalized();
    bool playingIdle = false;
    player.flipHorizontal = false;
    player.flipVertical = false;
    if(direction.x == 0 && direction.y == 0)
    {
        switch(player.faceOrientation)
        {
        case FaceOrientation::Up:
            player.changeAnimation(PlayerAnim_IdleUp);
            break;
        case FaceOrientation::Left:
            player.changeAnimation(PlayerAnim_IdleRight);
            player.flipHorizontal = true;
            break;
        case FaceOrientation::Right:
            player.changeAnimation(PlayerAnim_IdleRight);
            break;
        case FaceOrientation::Down:
            player.changeAnimation(PlayerAnim_IdleDown);
        default:
            break;
        }
        playingIdle = true;
    }
    else if(abs(direction.y) > abs(direction.x) )
    {
        // Vertical movement
        if(direction.y < 0)
        {
            player.changeAnimation(PlayerAnim_WalkDown);
            player.faceOrientation = FaceOrientation::Down;
        }
        else
        {
            player.changeAnimation(PlayerAnim_WalkUp);
            player.faceOrientation = FaceOrientation::Up;
        }
    }
    else
    {
        // Horizontal movement
        if(direction.x < 0)
        {
            player.changeAnimation(PlayerAnim_WalkRight);
            player.flipHorizontal = true;
            player.faceOrientation = FaceOrientation::Left;
        }
        else
        {
            player.changeAnimation(PlayerAnim_WalkRight);
            player.faceOrientation = FaceOrientation::Right;
        }
    }

    auto playerSpeed = 5*KmHr2MS;
    bool isRunning = global.controllerState.getButton(ControllerButton::X);
    if(isRunning)
    {
        playerSpeed *= 3;
        if(!playingIdle)
            player.animationState.playRate = 2.0f;
    }
    else
    {
        player.animationState.playRate = 1.0f;
    }

    player.velocity = direction*playerSpeed;

    auto deltaX = Vector2(player.velocity.x*delta, 0);
    if(canEntityMoveToPoint(player, player.position + deltaX))
        player.position += deltaX;

    auto deltaY = Vector2(0, player.velocity.y*delta);
    if(canEntityMoveToPoint(player, player.position + deltaY))
        player.position += deltaY;

    updateEntityAnimation(delta, player);
}

void updateMap(float delta)
{
    constexpr float MapTileFPS = 1.25;

    global.map.animationVariant = global.currentTime*MapTileFPS;

}

void update(float delta, const ControllerState &controllerState)
{
    initializeGlobalState();

    // Store the current time and update the controller state.
    global.currentTime += delta;
    global.oldControllerState = global.controllerState;
    global.controllerState = controllerState;

    updateMap(delta);
    updatePlayer(delta, global.player);
    global.camera.position = global.player.position;
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
