#include "GameInterface.hpp"
#include "GameLogic.hpp"
#include "Renderer.hpp"
#include "SoundSamples.hpp"
#include <algorithm>
#include <stdio.h>
#include <time.h>

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

static constexpr float BellyDecreaseSpeed = 0.50f;
static constexpr float EmptyStomachHurtSpeed = 2.0f;

static const TileOccupant TurretDestructionDropItems[] = {
    TileOccupant::None,
    TileOccupant::None,
    TileOccupant::None,
    TileOccupant::None,
    TileOccupant::Bullet,
    TileOccupant::Medkit,
};

static void placeSpecialItem(size_t x, size_t y, TileOccupant item)
{
    auto tileIndex = global.map.tileIndexAtRowColumn(y, x);
    global.map.occupants[tileIndex] = item;
    global.map.occupantStates[tileIndex].setDefault(item);
}
static void placeSpecialItems()
{
    placeSpecialItem(157, 54, TileOccupant::DemolitionBullet);
    placeSpecialItem(207, 121, TileOccupant::DemolitionBullet);
    placeSpecialItem(139, 139, TileOccupant::DemolitionBullet);
    placeSpecialItem(160, 49, TileOccupant::Flippers);

    placeSpecialItem(180, 194, TileOccupant::Torch);
    placeSpecialItem(245, 181, TileOccupant::InflatableBoat);
    placeSpecialItem(455, 178, TileOccupant::MotorBoat);
    placeSpecialItem(113, 85, TileOccupant::HolyProtection);
    placeSpecialItem(0, 5, TileOccupant::HellGate);
}

static void initializePlayer(PlayerState &player)
{
    player.spriteType = SpriteType::Character;
    player.animationState = PlayerAnim_IdleDown;

    player.position = Vector2(180, 130);
    player.boundingBox = Box2::fromCenterAndExtent(Vector2(), Vector2(1.0, 1.5));
    player.collisionBoundingBox = player.boundingBox;
    player.feetBoundingBox = Box2(player.boundingBox.min, player.boundingBox.min + Vector2(player.boundingBox.width(), player.boundingBox.height()/2))
                            .shinkBy(Vector2(0.1, 0.0));
    player.tileMovementMask = TileTypeMask::AnyGround;

    player.health = 100;
    player.belly = 25;
    player.bullets = 0;
}

static bool canEntityMoveToTileAtPoint(const Entity &entity, const Vector2 &targetPoint)
{
    auto tileIndex = global.map.tileIndexAtPoint(targetPoint);
    auto tileType = global.map.tiles[tileIndex];
    auto occupant = global.map.occupants[tileIndex];
    return isTileTypeInSet(tileType, entity.tileMovementMask) && isPassableOccupant(occupant);
}

static bool canEntityMoveToPoint(const Entity &entity, const Vector2 &targetPoint)
{
    return
        canEntityMoveToTileAtPoint(entity, entity.feetBoundingBox.bottomLeft() + targetPoint) &&
        canEntityMoveToTileAtPoint(entity, entity.feetBoundingBox.bottomRight() + targetPoint) &&
        canEntityMoveToTileAtPoint(entity, entity.feetBoundingBox.topRight() + targetPoint) &&
        canEntityMoveToTileAtPoint(entity, entity.feetBoundingBox.topLeft() + targetPoint) &&
        canEntityMoveToTileAtPoint(entity, entity.feetBoundingBox.center() + targetPoint); // Is this needed?
}

static void initializeGlobalState()
{
    if(global.isInitialized)
        return;

    global.random.seed = time(nullptr)^rand();

    global.map.loadFromFile("assets/earth_map.png");
    global.mapTileSet.loadFromFile("assets/tiles.png");
    global.characterTileSet.loadFromFile("assets/character-sprites.png");
    global.spriteSet.loadFromFile("assets/sprites.png");
    global.minimap.loadFromFile("assets/minimap.png");

    initializePlayer(global.player);
    placeSpecialItems();

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
        if(global.decayStage == DecayStage::Dead)
        {
            player.receiveDamage(5);
            global.somethingExploded = true;
        }
        break;
    case TileOccupant::Meat:
        player.increaseBelly(20);
        if(global.decayStage == DecayStage::Dying)
        {
            player.receiveDamage(10);
            global.somethingExploded = true;
        }
        else if(global.decayStage == DecayStage::Dead)
        {
            player.receiveDamage(50);
            global.somethingExploded = true;
        }
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
        player.demolitionBullets += 1;
        break;
    case TileOccupant::TripleDemolitionBullet:
        player.demolitionBullets += 10;
        break;
    case TileOccupant::Flippers:
        player.tileMovementMask |= TileTypeMask::ShallowWater;
        break;
    case TileOccupant::InflatableBoat:
        player.tileMovementMask |= TileTypeMask::ShallowWater | TileTypeMask::Water;
        break;
    case TileOccupant::MotorBoat:
        player.tileMovementMask |= TileTypeMask::ShallowWater | TileTypeMask::Water | TileTypeMask::DeepWater;
        break;
    case TileOccupant::Torch:
        player.hasIceProtection = true;
        break;
    case TileOccupant::HolyProtection:
        player.hasHolyProtection = true;
        break;
    case TileOccupant::HellGate:
        // Do not remove the gate.
        global.isGameCompleted = true;
        return;
    default:
        printf("TODO: Picked item %d\n", int(occupant));
        break;
    }

    occupant = TileOccupant::None;
    global.itemWasPicked = true;
}

static void updateAlivePlayerMovement(float delta, PlayerState &player)
{
    auto rawDirection = Vector2(global.controllerState.leftXAxis, global.controllerState.leftYAxis);
    auto directionLength = rawDirection.length();
    Vector2 direction = rawDirection;
    if(directionLength > 1.0f)
        direction = rawDirection * (1.0f / directionLength);

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

    auto playerSpeed = 2.0;
    player.running = global.controllerState.getButton(ControllerButton::A);
    if(player.running)
        playerSpeed *= 2.5;
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
    bool receiveHolyDamage = false;
    bool receiveIceDamage = false;
    bool needsBoat = false;;
    entityTouchingTilesDo(player, [&](size_t tileIndex) {
        auto &type = global.map.tiles[tileIndex];
        auto &occupant = global.map.occupants[tileIndex];

        // Interact with the items.
        if(isTileOccupantAnItem(occupant))
        {
            pickPlayerItem(player, type, occupant);
        }
        else if(type == TileType::HolyBarrier && !player.hasHolyProtection)
        {
            receiveHolyDamage = true;
        }
        else if(type == TileType::Ice && !player.hasIceProtection)
        {
            receiveIceDamage = true;
        }
        else if(type == TileType::Water || type == TileType::DeepWater)
        {
            needsBoat = true;
        }
    });

    player.inBoat = needsBoat && isTileTypeInSet(TileType::Water, player.tileMovementMask);

    if(receiveHolyDamage)
        player.receiveDamage(50*delta);
    else if(receiveIceDamage)
        player.receiveDamage(25*delta);
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

    global.shotWasFired = true;
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
    if(global.isPaused || global.isGameCompleted)
        return;

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
    if(global.matchTime > 10*60)
        global.decayStage = DecayStage::Dead;
    else if(global.matchTime > 5*60)
        global.decayStage = DecayStage::Dying;
    else
        global.decayStage = DecayStage::Normal;
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

    // Check whether is there something interesting on this tile.
    auto tileIndex = global.map.tileIndexAtPoint(bullet.position);
    auto &tileType = global.map.tiles[tileIndex];
    auto &occupant = global.map.occupants[tileIndex];
    auto &occupantState = global.map.occupantStates[tileIndex];

    if(!bullet.isHighBullet() && (tileType == TileType::Rock || tileType == TileType::DevilStone))
    {
        bullet.gotTarget();
        if(bullet.isDemolition() && tileType == TileType::Rock)
        {
            tileType = TileType::Earth;
            global.somethingExploded = true;
        }
    }

    // Turret do not hurt other turrets.
    if(isTileOccupantAStructure(occupant) && !bullet.wasFiredByTurret())
    {
        bullet.gotTarget();
        occupantState.generic.health = std::max(0, int(occupantState.generic.health - bullet.power));
        if(occupantState.generic.health == 0)
        {
            tileOccupantDestroyed(occupant, occupantState);
            global.somethingExploded = true;
        }
    }
}

static void updateBullets(float delta)
{
    if(global.isPaused)
        return;

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

    if(type == TileType::Rock || type == TileType::DevilStone)
        return true;

    auto position = start;
    for(int i = 0; i < maxSteps; ++i, position += step)
    {
        size_t tileIndex = global.map.tileIndexAtPoint(position);
        auto tile = global.map.tiles[tileIndex];
        if(tile == TileType::Rock || tile == TileType::DevilStone)
            return false;

        auto depth = (position - start).dot(direction);
        if(depth >= playerDirectionAmount)
            break;
    }

    return true;
}

static bool turretAttack(float delta, int row, int column, TileType type, TileOccupantState &state)
{
    if(global.isGameCompleted)
        return false;

    auto &turret = state.turret;
    bool isDiagonal = turret.renderState & 1;
    bool result = false;
    Vector2 position(column + 0.5f, row + 0.5f);
    Vector2 fireDirection;

#define ATTEMPT_FIRE_DIRECTION(dx, dy) \
        if(!result) \
        { \
            fireDirection = Vector2(dx, dy); \
            result = castPlayerVisibleRay(position, fireDirection, type); \
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
            uint32_t flags = BulletFlags::FiredByTurret;
            if(type == TileType::Rock || type == TileType::DevilStone)
                flags |= BulletFlags::HighBullet;

            fireBullet(2.0f, position + fireDirection*0.5, fireDirection.normalized()*10.0f, Box2::fromCenterAndExtent(Vector2(), Vector2(0.1875f, 0.1875f)),
                0xFFCCCCCC, 0xFFFFFFFF, flags, 1);
            turret.cooldown = 400;
        }
    }

    return result;
}

static void updateTurret(float delta, int row, int column, TileType type, TileOccupantState &state)
{
    if(global.isPaused)
        return;

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

static void updateTorch(float delta, TileOccupantState &state)
{
    state.generic.milliseconds += delta*1000;
    state.generic.renderState = (state.generic.milliseconds % 300) >= 150 ? 1 : 0;
}

static void updateHellGate(float delta, TileOccupantState &state)
{
    state.generic.milliseconds += delta*1000;
    state.generic.renderState = (state.generic.milliseconds % 300) >= 150 ? 1 : 0;
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
    case TileOccupant::Torch:
        updateTorch(delta, occupantState);
        break;
    case TileOccupant::HellGate:
        updateHellGate(delta, occupantState);
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

static void doCheating()
{
    auto &player = global.player;
    return;
    //global.decayStage = DecayStage::Normal;
    //global.decayStage = DecayStage::Dying;
    //global.decayStage = DecayStage::Dead;

    //printf("player position %f %f\n", player.position.x, player.position.y);
    //player.position = Vector2(0, 7); // The ending sanctum

    player.health = 100; // God mode

    player.hasHolyProtection = true;
    player.hasIceProtection = true;

    // Jesus mode
    player.tileMovementMask |= TileTypeMask::Water | TileTypeMask::ShallowWater | TileTypeMask::DeepWater;
}

void update(float delta, const ControllerState &controllerState)
{
    initializeGlobalState();

    // Pause button
    if(global.isButtonPressed(ControllerButton::Start))
        global.isPaused = !global.isPaused;

    // Store the current time and update the controller state.
    global.currentTime += delta;
    if(!global.isPaused && !global.isGameCompleted)
        global.matchTime += delta;
    global.oldControllerState = global.controllerState;
    global.controllerState = controllerState;

    global.shotWasFired = false;
    global.itemWasPicked = false;
    global.somethingExploded = false;

    updateMap(delta);
    doCheating();

    updatePlayer(delta, global.player);
    updateScreenTileOccupants(delta);
    updateBullets(delta);

    if(global.shotWasFired)
        playShotSound(global.random.next32());
    if(global.itemWasPicked)
        playPickSound(global.random.next32());
    if(global.somethingExploded)
        playExplosionSound(global.random.next32());

    global.camera.position = global.player.position;
}

void Entity::receiveDamage(float damage)
{
    bool wasAlive = isAlive();
    health = std::max(health - damage, 0.0f);
    if(wasAlive && !isAlive())
        global.somethingExploded = true;
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
