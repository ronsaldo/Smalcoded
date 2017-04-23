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
static const AnimationState PlayerAnim_DeadIdle = {0, 14, 2, 2, true, 0.5f};

static constexpr float BellyDecreaseSpeed = 0.25f;
static constexpr float EmptyStomachHurtSpeed = 2.0f;

static const TileOccupant TurretDestructionDropItems[] = {
    TileOccupant::None,
    TileOccupant::None,
    TileOccupant::None,
    TileOccupant::None,
    TileOccupant::Bullet,
    TileOccupant::Medkit,
};

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

    player.health = 100;
    player.belly = 25;
    player.bullets = 0;
}

bool canEntityMoveToTileAtPoint(const Entity &entity, const Vector2 &targetPoint)
{
    auto tileIndex = global.map.tileIndexAtPoint(targetPoint);
    auto tileType = global.map.tiles[tileIndex];
    auto occupant = global.map.occupants[tileIndex];
    return isTileTypeInSet(tileType, entity.tileMovementMask) && isPassableOccupant(occupant);
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
    global.spriteSet.loadFromFile("assets/sprites.png");
    global.minimap.loadFromFile("assets/minimap.png");
    initializePlayer(global.player);

    global.numberOfDeadBullets = MaxNumberOfBullets;
    for(size_t i = 0; i < MaxNumberOfBullets; ++i)
        global.deadBullets[i] = i;

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

template<typename FT>
void boxTouchingTilesDo(const Box2 &box, const FT &f)
{
    Vector2 positions[5] = {
        box.bottomLeft(),
        box.bottomRight(),
        box.topRight(),
        box.topLeft(),
        box.center()
    };

    int usedTiles[5];
    int usedTileCount = 0;

    for(int i = 0; i < 5; ++i)
    {
        const auto &point = positions[i];
        auto pointTile = global.map.tileIndexAtPoint(point);
        bool used = false;
        for(int j = 0; j < usedTileCount; ++j)
        {
            if(usedTiles[j] == pointTile)
            {
                used = true;
                break;
            }
        }

        if(!used)
        {
            usedTiles[usedTileCount++] = pointTile;
            f(pointTile);
        }
    }
}

template<typename FT>
void entityTouchingTilesDo(Entity &entity, const FT &f)
{
    boxTouchingTilesDo(entity.feetBoundingBox.translatedBy(entity.position), f);
}

void pickPlayerItem(PlayerState &player, TileType &type, TileOccupant &occupant)
{
    switch(occupant)
    {
    case TileOccupant::Apple:
        player.increaseBelly(10);
        break;
    case TileOccupant::Meat:
        player.increaseBelly(20);
        break;
    case TileOccupant::MilitaryMeal:
        player.increaseBelly(50);
        break;
    case TileOccupant::Medkit:
        player.increaseHealth(10);
        break;
    case TileOccupant::Bullet:
        player.bullets += 10;
        break;
    case TileOccupant::TripleBullet:
        player.bullets += 40;
        break;
    case TileOccupant::DemolitionBullet:
        player.demolitionBullets += 3;
        break;
    case TileOccupant::TripleDemolitionBullet:
        player.demolitionBullets += 10;
        break;
    default:
        printf("TODO: Pick item %d\n", int(occupant));
        break;
    }

    occupant = TileOccupant::None;
}

static void updateAlivePlayerMovement(float delta, PlayerState &player)
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
    player.running = global.controllerState.getButton(ControllerButton::A);
    if(player.running)
        playerSpeed *= 3;
    player.velocity = direction*playerSpeed;

    player.animationState.playRate = player.isActuallyRunning() ? 2.0f : 1.0f;

    auto deltaX = Vector2(player.velocity.x*delta, 0);
    if(canEntityMoveToPoint(player, player.position + deltaX))
        player.position += deltaX;

    auto deltaY = Vector2(0, player.velocity.y*delta);
    if(canEntityMoveToPoint(player, player.position + deltaY))
        player.position += deltaY;
    player.position = normalizeWorldCoordinate(player.position);

    // Interact with the touching tiles.
    entityTouchingTilesDo(player, [&](size_t tileIndex) {
        auto &type = global.map.tiles[tileIndex];
        auto &occupant = global.map.occupants[tileIndex];

        // Interact with the items.
        if(isTileOccupantAnItem(occupant))
        {
            pickPlayerItem(player, type, occupant);
        }
    });
}

static void fireBullet(float timeToLive, Vector2 position, Vector2 velocity, Box2 boundingBox, uint32_t color, uint32_t flashColor, uint32_t flags, float power)
{
    if(global.numberOfDeadBullets == 0)
        return;

    // Allocate a bullet.
    auto bulletIndex = global.deadBullets[--global.numberOfDeadBullets];
    global.aliveBullets[global.numberOfAliveBullets++] = bulletIndex;

    auto &bullet = global.bullets[bulletIndex];
    bullet.timeToLive = timeToLive;
    bullet.position = position;
    bullet.velocity = velocity;
    bullet.boundingBox = boundingBox;
    bullet.color = color;
    bullet.flashColor = flashColor;
    bullet.flags = flags;
    bullet.power = power;
}

inline Vector2 bulletOffsetForFaceOrientation(FaceOrientation orientation)
{
    switch(orientation)
    {
    case FaceOrientation::Left: return Vector2(-0.5f, 0.0f);
    case FaceOrientation::Right: return Vector2(0.5f, 0.0f);
    default: return Vector2(0, 0);
    }
}

static void updateAlivePlayer(float delta, PlayerState &player)
{
    if(global.isButtonPressed(ControllerButton::LeftShoulder) ||
        global.isButtonPressed(ControllerButton::RightShoulder) ||
        global.isButtonPressed(ControllerButton::B))
    {
        player.withDemolitionBullets = !player.withDemolitionBullets;
    }

    if(global.isButtonPressed(ControllerButton::X) || global.isButtonPressed(ControllerButton::RightTrigger))
    {
        auto bulletPosition = player.position + bulletOffsetForFaceOrientation(player.faceOrientation);

        // TODO: Check whether we can fire the bullet or not.

        auto lifeTime = 2.0f;
        if(player.withDemolitionBullets && player.demolitionBullets > 0)
        {
            fireBullet(lifeTime, bulletPosition, player.velocity + player.fireDirection()*15, Box2::fromCenterAndExtent(Vector2(), Vector2(0.1875f, 0.1875f)),
                0xFF00CC00, 0xFF00FF00, BulletFlags::FiredByPlayer | BulletFlags::Demolition, 100);
            --player.demolitionBullets;
        }
        else if(!player.withDemolitionBullets && player.bullets > 0)
        {
            fireBullet(lifeTime, bulletPosition, player.velocity + player.fireDirection()*10, Box2::fromCenterAndExtent(Vector2(), Vector2(0.1875f, 0.1875f)),
                0xFF00CCCC, 0xFF00FFFF, BulletFlags::FiredByPlayer, 35);
            --player.bullets;
        }
    }

    updateAlivePlayerMovement(delta, player);

    // Hunger
    auto bellyDecreaseSpeed = BellyDecreaseSpeed;
    if(player.isActuallyRunning())
        bellyDecreaseSpeed *= 2;
    player.belly = std::max(player.belly - bellyDecreaseSpeed * delta, 0.0f);
    if(player.roundedBelly() == 0)
    {
        auto hungerDamage = EmptyStomachHurtSpeed;
        if(player.isActuallyRunning())
            hungerDamage *= 2;

        player.receiveDamage(hungerDamage*delta);
    }
}

void updatePlayer(float delta, PlayerState &player)
{
    if(player.isAlive())
    {
        updateAlivePlayer(delta, player);
    }
    else
    {
        player.changeAnimation(PlayerAnim_DeadIdle);
        player.flipHorizontal = false;
        player.flipVertical = false;
    }

    updateEntityAnimation(delta, player);
}

static void updateMap(float delta)
{
    constexpr float MapTileFPS = 1.25;

    global.map.animationVariant = global.currentTime*MapTileFPS;
}

static void tileOccupantDestroyed(TileOccupant &occupant, TileOccupantState &occupantState)
{
    switch(occupant)
    {
    case TileOccupant::Turret:
        occupant = TurretDestructionDropItems[global.random.next32() % arrayLength(TurretDestructionDropItems)];
        break;
    default:
        occupant = TileOccupant::None;
        break;
    }

    occupantState.setDefault(occupant);
}

static void checkBulletCollisions(BulletState &bullet)
{
    // Attempt to kill the player...
    //printf("bullet pos %f %f bbox: %f %f - %f %f\n", bullet.position.x, bullet.position.y, bullet.boundingBox.min.x, bullet.boundingBox.min.y, bullet.boundingBox.max.x, bullet.boundingBox.max.y);
    if(!bullet.wasFiredByPlayer() && global.player.collisionBoundingBox.containsPoint(bullet.position - global.player.position))
    {
        global.player.receiveDamage(bullet.power);
        bullet.gotTarget();
        return;
    }

    // Turret do not hurt other turrets.
    if(!bullet.wasFiredByTurret())
    {
        // Check whether is there something interesting on this tile.
        auto tileIndex = global.map.tileIndexAtPoint(bullet.position);
        auto &tileType = global.map.tiles[tileIndex];
        auto &occupant = global.map.occupants[tileIndex];
        auto &occupantState = global.map.occupantStates[tileIndex];

        if(tileType == TileType::Rock)
        {
            bullet.gotTarget();
            if(bullet.isDemolition())
                tileType = TileType::Earth;
        }

        if(isTileOccupantAStructure(occupant))
        {
            bullet.gotTarget();
            occupantState.generic.health = std::max(0, int(occupantState.generic.health - bullet.power));
            if(occupantState.generic.health == 0)
                tileOccupantDestroyed(occupant, occupantState);
        }
    }
}

static void updateBullets(float delta)
{
    // Work in a temporary copy.
    int bulletsToUpdate[MaxNumberOfBullets];
    int numberOfBulletsToUpdate = global.numberOfAliveBullets;
    memcpy(bulletsToUpdate, global.aliveBullets, global.numberOfAliveBullets*sizeof(4));

    for(int i = 0; i < numberOfBulletsToUpdate; ++i)
    {
        auto bulletIndex = bulletsToUpdate[i];
        auto &bullet = global.bullets[bulletIndex];
        bullet.timeToLive -= delta;
        bullet.position += bullet.velocity * delta;

        // Try to hit something.
        if(bullet.isAlive())
            checkBulletCollisions(bullet);
    }

    // Store back the new dead and alive bullets
    global.numberOfAliveBullets = 0;
    for(int i = 0; i < numberOfBulletsToUpdate; ++i)
    {
        auto bulletIndex = bulletsToUpdate[i];
        auto &bullet = global.bullets[bulletIndex];
        if(bullet.isAlive())
        {
            global.aliveBullets[global.numberOfAliveBullets++] = bulletIndex;
        }
        else
        {
            global.deadBullets[global.numberOfDeadBullets++] = bulletIndex;
        }
    }

}

static bool castPlayerVisibleRay(const Vector2 &start, const Vector2 &step, TileType type = TileType::None, int maxSteps = 30)
{
    auto direction = step.normalized(); // Maybe the normalization is not needed.
    auto playerPosition = start - global.player.position;

    if(!global.player.collisionBoundingBox.isIntersectedByLine(playerPosition, playerPosition + step))
        return false;

    auto playerDirectionAmount = (global.player.position - start).dot(direction);
    if(playerDirectionAmount < 0)
        return false;

    if(type == TileType::Rock)
        return true;

    auto position = start;
    for(int i = 0; i < maxSteps; ++i, position += step)
    {
        size_t tileIndex = global.map.tileIndexAtPoint(position);
        if(global.map.tiles[tileIndex] == TileType::Rock)
            return false;

        auto depth = (position - start).dot(direction);
        if(depth >= playerDirectionAmount)
            break;
    }

    return true;
}

static bool turretAttack(float delta, int row, int column, TileType type, TileOccupantState &state)
{
    auto &turret = state.turret;
    bool isDiagonal = turret.renderState & 1;
    bool result = false;
    Vector2 position(column + 0.5f, row + 0.5f);
    Vector2 fireDirection;

#define ATTEMPT_FIRE_DIRECTION(dx, dy) \
        if(!result) \
        { \
            fireDirection = Vector2(dx, dy); \
            result = castPlayerVisibleRay(position, fireDirection); \
        }

    if(isDiagonal)
    {
        ATTEMPT_FIRE_DIRECTION(-1, 1);
        ATTEMPT_FIRE_DIRECTION(1, 1);
        ATTEMPT_FIRE_DIRECTION(-1, -1);
        ATTEMPT_FIRE_DIRECTION(1, -1);
    }
    else
    {
        ATTEMPT_FIRE_DIRECTION(-1, 0);
        ATTEMPT_FIRE_DIRECTION(1, 0);
        ATTEMPT_FIRE_DIRECTION(0, -1);
        ATTEMPT_FIRE_DIRECTION(0, 1);
    }
#undef ATTEMPT_FIRE_DIRECTION

    if(result)
    {
        turret.milliseconds = isDiagonal ? 0 : 500;
        if(!turret.cooldown)
        {
            fireBullet(2.0f, position + fireDirection*0.5, fireDirection.normalized()*10.0f, Box2::fromCenterAndExtent(Vector2(), Vector2(0.1875f, 0.1875f)),
                0xFFCCCCCC, 0xFFFFFFFF, BulletFlags::FiredByTurret, 1);
            turret.cooldown = 400;
        }
    }

    return result;
}

static void updateTurret(float delta, int row, int column, TileType type, TileOccupantState &state)
{
    auto &turret = state.turret;
    auto isDiagonal = turret.renderState & 1;
    turret.cooldown = std::max(0, int(turret.cooldown - delta*1000));

    if(!turretAttack(delta, row, column, type, state))
    {
        turret.milliseconds += delta*1000;
        turret.renderState = (turret.milliseconds % 1000) >= 500 ? 1 : 0;

        // If we changed of position, try to perform a new attack
        auto isNowDiagonal = turret.renderState & 1;
        if(isDiagonal != isNowDiagonal)
            turretAttack(delta, row, column, type, state);
    }
}

static void updateScreenTileOccupant(float delta, int row, int column)
{
    auto tileIndex = global.map.tileIndexAtRowColumn(row, column);
    auto type = global.map.tiles[tileIndex];
    auto &occupant = global.map.occupants[tileIndex];
    auto &occupantState = global.map.occupantStates[tileIndex];

    if(occupant == TileOccupant::None)
        return;

    switch(occupant)
    {
    case TileOccupant::Turret:
        updateTurret(delta, row, column, type, occupantState);
        break;
    default:
        break;
    }
}

static void updateScreenTileOccupants(float delta)
{
    global.map.screenTilesDo([=](int row, int column) {
        updateScreenTileOccupant(delta, row, column);
    });
}

void update(float delta, const ControllerState &controllerState)
{
    initializeGlobalState();

    // Store the current time and update the controller state.
    global.currentTime += delta;
    global.matchTime += delta;
    global.oldControllerState = global.controllerState;
    global.controllerState = controllerState;

    // Pause button
    if(global.isButtonPressed(ControllerButton::Start))
        global.isPaused = !global.isPaused;

    updateMap(delta);
    if(!global.isPaused)
    {
        updatePlayer(delta, global.player);
        updateScreenTileOccupants(delta);
        updateBullets(delta);
    }
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
