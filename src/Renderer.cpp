#include "Renderer.hpp"
#include "GameLogic.hpp"
#include <math.h>
#include <algorithm>
#include <stdio.h>

static const Rectangle TileOccupantSprites[] = {
    {0, 0, 32, 32},

    // Nasty stuff
    /*Turret*/ {0, 4*32, 32, 32},

    // Items
    /* Apple */ {2*32, 4*32, 32, 32},
    /* Meat */ {3*32, 4*32, 32, 32},
    /* MilitaryMeal*/ {4*32, 4*32, 32, 32},
    /* Medkit*/ {9*32, 4*32, 32, 32},
    /* Bullet */{5*32, 4*32, 32, 32},
    /* TripleBullet */{6*32, 4*32, 32, 32},
    /* DemolitionBullet */{7*32, 4*32, 32, 32},
    /* TripleDemolitionBullet */{8*32, 4*32, 32, 32},

    // Special Items
    /* Flippers */{10*32, 4*32, 32, 32},
    /* Torch */{11*32, 4*32, 32, 32},
    /*InflatableBoat*/{13*32, 4*32, 32, 32},
    /*MotorBoat*/{0*32, 5*32, 32, 32},
    /*HolyProtection*/{1*32, 5*32, 32, 32},
    /*HellGate*/{2*32, 5*32, 32, 32},
};

static const Rectangle BoatBack = {14*32, 4*32, 32, 32};
static const Rectangle BoatFront = {15*32, 4*32, 32, 32};

static const char FontCharacters[] = "0123456789?!+-@#()ABCDEFGHIJKLMNOPQRSTUVWXYZ$";
static const uint16_t FontTileSize = 32;
static uint16_t FontCharacterMap[256][2];

static struct FontCharacterMapBuilder
{
    FontCharacterMapBuilder()
    {
        auto constexpr Columns = 512 / FontTileSize;

        int characterIndex = 1;
        for(size_t i = 0; i < sizeof(FontCharacters); ++i, ++characterIndex)
        {
            int row = characterIndex / Columns;
            int column = characterIndex % Columns;
            int character = FontCharacters[i];

            int x = column*FontTileSize;
            int y = row*FontTileSize;
            FontCharacterMap[character][0] = x;
            FontCharacterMap[character][1] = y;
            if('A' <= character && character <= 'Z')
            {
                auto lower = character - 'A' + 'a';
                FontCharacterMap[lower][0] = x;
                FontCharacterMap[lower][1] = y;
            }
        }
    }
} fontCharacterMapBuilder;

inline int clampCoordinate(int min, int max, int x)
{
    if(x < min)
        return min;
    else if(x > max)
        return max;
    else
        return x;
}

inline Vector2 viewToScreen(const Framebuffer &framebuffer, const Vector2 &v)
{
    return units2Pixels(v) + Vector2(framebuffer.width/2, framebuffer.height/2);
}

inline Vector2 screenToView(const Framebuffer &framebuffer, const Vector2 &v)
{
    return pixels2Units(v - Vector2(framebuffer.width/2, framebuffer.height/2));
}

inline Vector2 worldToView(const Vector2 &v)
{
    return v - global.camera.position;
}

inline Vector2 viewToWorld(const Vector2 &v)
{
    return v + global.camera.position;
}

inline Vector2 screenToWorld(const Framebuffer &framebuffer, const Vector2 &v)
{
    return viewToWorld(screenToView(framebuffer, v));
}

inline Vector2 worldToScreen(const Framebuffer &framebuffer, const Vector2 &v)
{
    return viewToScreen(framebuffer, worldToView(v));
}

inline Vector2 screenToWorld(const Framebuffer &framebuffer, int x, int y)
{
    return screenToWorld(framebuffer, Vector2(x, y));
}

inline Vector2 worldToScreen(const Framebuffer &framebuffer, int x, int y)
{
    return worldToScreen(framebuffer, Vector2(x, y));
}

static void drawRectangle(const Framebuffer &framebuffer, uint32_t color, int x, int y, int width, int height)
{
    auto minX = x;
    auto minY = y;
    auto maxX = x + width;
    auto maxY = y + height;

    minX = clampCoordinate(0, framebuffer.width, minX);
    maxX = clampCoordinate(0, framebuffer.width, maxX);
    minY = clampCoordinate(0, framebuffer.height, minY);
    maxY = clampCoordinate(0, framebuffer.height, maxY);
    //printf("rctangle size: %d %d\n", width, height);

    auto rowStart = framebuffer.pixels + minY* framebuffer.pitch;
    for(int dy = minY; dy < maxY; ++dy, rowStart += framebuffer.pitch)
    {
        auto row = reinterpret_cast<uint32_t*> (rowStart);
        for(int dx = minX; dx < maxX; ++dx)
            row[dx] = color;
    }
}

template<typename TileSetImageType>
static void blitTileRectangle(const Framebuffer &framebuffer, int destX, int destY, const TileSetImageType &tileSet, const Rectangle &rectangle, bool flipHorizontal=false, bool flipVertical=false)
{
    auto minX = destX;
    auto minY = destY;
    auto maxX = destX + rectangle.width;
    auto maxY = destY + rectangle.height;

    if(minX >= framebuffer.width || minY >= framebuffer.height)
        return;

    minX = clampCoordinate(0, framebuffer.width, minX);
    maxX = clampCoordinate(0, framebuffer.width, maxX);
    minY = clampCoordinate(0, framebuffer.height, minY);
    maxY = clampCoordinate(0, framebuffer.height, maxY);

    auto offsetX = minX - destX;
    auto offsetY = minY - destY;
    if(offsetX >= rectangle.width || offsetY >= rectangle.height)
        return;

    auto rowStart = framebuffer.pixels + minY* framebuffer.pitch;
    auto sourceStart = tileSet.data + rectangle.y * TileSetImageType::Width + rectangle.x;
    if(flipVertical)
        sourceStart += (rectangle.height - 1) * TileSetImageType::Width;
    else
        sourceStart += offsetY * TileSetImageType::Width;
    if(flipHorizontal)
        sourceStart += rectangle.width - 1;
    else
        sourceStart += offsetX;

    auto sourceRowDelta = flipHorizontal ? -1 : 1;
    for(int dy = minY; dy < maxY; ++dy, rowStart += framebuffer.pitch, sourceStart += TileSetImageType::Width)
    {
        auto row = reinterpret_cast<uint32_t*> (rowStart);
        auto sourceRow = sourceStart;
        for(int dx = minX; dx < maxX; ++dx, sourceRow += sourceRowDelta)
        {
            auto color = *sourceRow;
            if((color & 0xFF000000) != 0) // Alpha testing
                row[dx] = color;
        }
    }
}

template<typename TileSetImageType>
static void blitTileRectangleWithColor(const Framebuffer &framebuffer, uint32_t color, int destX, int destY, const TileSetImageType &tileSet, const Rectangle &rectangle, bool flipHorizontal=false, bool flipVertical=false)
{
    auto minX = destX;
    auto minY = destY;
    auto maxX = destX + rectangle.width;
    auto maxY = destY + rectangle.height;

    if(minX >= framebuffer.width || minY >= framebuffer.height)
        return;

    minX = clampCoordinate(0, framebuffer.width, minX);
    maxX = clampCoordinate(0, framebuffer.width, maxX);
    minY = clampCoordinate(0, framebuffer.height, minY);
    maxY = clampCoordinate(0, framebuffer.height, maxY);

    auto offsetX = minX - destX;
    auto offsetY = minY - destY;
    if(offsetX >= rectangle.width || offsetY >= rectangle.height)
        return;

    auto rowStart = framebuffer.pixels + minY* framebuffer.pitch;
    auto sourceStart = tileSet.data + rectangle.y * TileSetImageType::Width + rectangle.x;
    if(flipVertical)
        sourceStart += (rectangle.height - 1) * TileSetImageType::Width;
    else
        sourceStart += offsetY * TileSetImageType::Width;
    if(flipHorizontal)
        sourceStart += rectangle.width - 1;
    else
        sourceStart += offsetX;

    auto sourceRowDelta = flipHorizontal ? -1 : 1;
    for(int dy = minY; dy < maxY; ++dy, rowStart += framebuffer.pitch, sourceStart += TileSetImageType::Width)
    {
        auto row = reinterpret_cast<uint32_t*> (rowStart);
        auto sourceRow = sourceStart;
        for(int dx = minX; dx < maxX; ++dx, sourceRow += sourceRowDelta)
        {
            auto sourceColor = *sourceRow;
            if((sourceColor & 0xFF000000) != 0) // Alpha testing
                row[dx] = color & sourceColor;
        }
    }
}

static void drawText(const Framebuffer &framebuffer, uint32_t color, int destX, int destY, const char *text)
{
    auto startX = destX;
    for(; *text; ++text)
    {
        int character = *text;
        if(character > ' ')
        {
            Rectangle rect(FontCharacterMap[character][0], FontCharacterMap[character][1], FontTileSize, FontTileSize);
            //printf("Draw char: %d %d - %d %d\n", rect.x, rect.y, rect.width, rect.height);
            blitTileRectangleWithColor(framebuffer, color, destX, destY, global.spriteSet, rect);
            destX += FontTileSize;
        }
        else
        {
            switch(character)
            {
            case ' ':
                destX += FontTileSize;
                break;
            case '\n':
                destX = startX;
                destY += FontTileSize;
                break;
            }
        }

    }
}

static void drawBox(const Framebuffer &framebuffer, uint32_t color, const Box2 &box)
{
    auto min = viewToScreen(framebuffer, box.min).floor();
    auto max = viewToScreen(framebuffer, box.max).floor();
    auto extent = max - min;

    drawRectangle(framebuffer, color, min.x, framebuffer.height - (min.y + extent.y) - 1, extent.x, extent.y);
}

static void renderBackground(const Framebuffer &framebuffer)
{
    auto minPosition = screenToWorld(framebuffer, 0, 0).floor();
    auto maxPosition = screenToWorld(framebuffer, framebuffer.width, framebuffer.height).ceil();

    auto offset = worldToScreen(framebuffer, minPosition);
    int offsetX = offset.x;
    int offsetY = offset.y;
    int pixelsPerTile = Units2Pixels;

    int minX = minPosition.x;
    int minY = minPosition.y;
    int maxX = maxPosition.x;
    int maxY = maxPosition.y;

    auto destY = offsetY;
    int decayStageOffset = int(global.decayStage)*2;

    //printf("offsets %d %d\n", offsetX, offsetY);
    for(int y = minY; y <= maxY; ++y, destY += pixelsPerTile)
    {
        auto tileRow = (floorModule(y, TileMap::Height)) * TileMap::Width;
        auto destX = offsetX;
        for(int x = minX; x <= maxX; ++x, destX += pixelsPerTile)
        {
            auto tx = floorModule(x, TileMap::Width);
            auto tileIndex = tileRow + tx;
            //printf("tile index: %d\n", tileIndex);
            auto tileType = global.map.tiles[tileIndex];

            //drawRectangle(framebuffer, color, destX, framebuffer.height - destY - pixelsPerTile, pixelsPerTile, pixelsPerTile);
            int animationVariant = Random::hashBit(global.map.animationVariant ^ global.map.tileRandom[tileIndex]);
            blitTileRectangle(framebuffer, destX, framebuffer.height -1 - (destY + pixelsPerTile) , global.mapTileSet,
                global.mapTileSet.getTileRectangle(int(tileType), animationVariant + decayStageOffset, pixelsPerTile, pixelsPerTile));

            // Draw the tile occupant
            auto occupant = global.map.occupants[tileIndex];
            auto occupantVariation = global.map.occupantStates[tileIndex].generic.renderState & 1;
            if(occupant != TileOccupant::None)
            {
                //printf("Draw occupant\n");
                auto spriteRectangle = TileOccupantSprites[int(occupant)];
                spriteRectangle.x += spriteRectangle.width*occupantVariation;
                blitTileRectangle(framebuffer, destX, framebuffer.height - 1 - (destY + spriteRectangle.height), global.spriteSet, spriteRectangle);
            }
        }
    }
}

static void renderEntity(const Framebuffer &framebuffer, const Entity &entity)
{
    auto spritePosition = worldToScreen(framebuffer, entity.position + entity.boundingBox.bottomLeft());
    auto destX = spritePosition.x;
    auto destY = spritePosition.y;

    switch(entity.spriteType)
    {
    case SpriteType::Tile:
        blitTileRectangle(framebuffer, destX, framebuffer.height - (destY + 32) - 1, global.mapTileSet, global.mapTileSet.getTileRectangle(entity.spriteRow, entity.spriteColumn, 32, 32),
        entity.flipHorizontal, entity.flipVertical);
        break;
    case SpriteType::Character:
        blitTileRectangle(framebuffer, destX, framebuffer.height - (destY + 48) - 1, global.characterTileSet, global.characterTileSet.getTileRectangle(entity.spriteRow, entity.spriteColumn, 32, 48),
        entity.flipHorizontal, entity.flipVertical);
        break;
    case SpriteType::None:
    default:
    //drawBox(framebuffer, encodeColor(255, 255, 255, 255), entity.boundingBox.translatedBy(translation));
        drawBox(framebuffer, encodeColor(255, 255, 255, 255), entity.boundingBox.translatedBy(worldToView(entity.position)));
        break;
    }
}

static void renderPlayer(const Framebuffer &framebuffer, const PlayerState &player)
{
    if(global.isGameCompleted)
        return;

    auto boatOffset = Vector2(0.0f, -0.2f);
    auto spritePosition = worldToScreen(framebuffer, player.position + player.boundingBox.bottomLeft() + boatOffset);
    auto destX = spritePosition.x;
    auto destY = framebuffer.height - (spritePosition.y + /*Sprite size */ 32 ) - 1;

    if(player.inBoat)
        blitTileRectangle(framebuffer, destX, destY, global.spriteSet, BoatBack);
    //printf("feetExtent %f %f\n", feetExtent.x, feetExtent.y);
    renderEntity(framebuffer, player);
    if(player.inBoat)
        blitTileRectangle(framebuffer, destX, destY, global.spriteSet, BoatFront);

}

static void renderEntities(const Framebuffer &framebuffer)
{
    renderPlayer(framebuffer, global.player);
}

static void renderMinimap(const Framebuffer &framebuffer)
{
    auto rectangle = global.minimap.wholeRectangle();
    auto x = framebuffer.width - rectangle.width;
    auto y = framebuffer.height - rectangle.height;

    blitTileRectangle(framebuffer, x, y, global.minimap, rectangle);

    auto cursorX = x + floor(global.player.position.x * rectangle.width / float(WorldWidth));
    auto cursorY = y + rectangle.height - floor(global.player.position.y * rectangle.height / float(WorldHeight));
    auto cursorWidth = 4;
    auto cursorHeight = 4;
    drawRectangle(framebuffer, 0xFF0000FF, cursorX - cursorWidth/2, cursorY - cursorHeight/2, cursorWidth, cursorHeight);
}

static void renderBullets(const Framebuffer &framebuffer)
{
    for(int i = 0; i < global.numberOfAliveBullets; ++i)
    {
        auto bulletIndex = global.aliveBullets[i];
        auto &bullet = global.bullets[bulletIndex];
        auto bulletColor = (int(bullet.timeToLive*10) & 1) != 0 ? bullet.color : bullet.flashColor;

        auto box = bullet.boundingBox.translatedBy(worldToView(bullet.position));
        drawBox(framebuffer, bulletColor, box);
    }
}

inline uint32_t colorForPercentageMeter(int percentage)
{
    if(percentage > 75)
        return 0xFFFF0000;
    else if(percentage > 50)
        return 0xFF00FF00;
    else if(percentage > 25)
        return 0xFF00FFFF;
    else
        return 0xFF0000FF;
}

static void renderHealth(const Framebuffer &framebuffer)
{
    char buffer[64];
    int health = global.player.roundedHealth();

    blitTileRectangle(framebuffer, 0, 0, global.spriteSet, TileOccupantSprites[int(TileOccupant::Medkit)]);

    sprintf(buffer, "%03d", health);
    drawText(framebuffer, colorForPercentageMeter(health), FontTileSize, 0, buffer);
}

static void renderBelly(const Framebuffer &framebuffer)
{
    char buffer[64];
    int belly = global.player.roundedBelly();

    blitTileRectangle(framebuffer, 0, FontTileSize, global.spriteSet, TileOccupantSprites[int(TileOccupant::Meat)]);

    sprintf(buffer, "%03d", belly);
    drawText(framebuffer, colorForPercentageMeter(belly), FontTileSize, FontTileSize, buffer);
}

static void renderAmmo(const Framebuffer &framebuffer)
{
    char buffer[64];

    auto bulletSprite = global.player.withDemolitionBullets ? TileOccupant::TripleDemolitionBullet : TileOccupant::TripleBullet;
    blitTileRectangle(framebuffer, 0, FontTileSize*2, global.spriteSet, TileOccupantSprites[int(bulletSprite)]);

    sprintf(buffer, "%03d", global.player.withDemolitionBullets ? global.player.demolitionBullets : global.player.bullets);
    drawText(framebuffer, 0xFF00FFFF, FontTileSize, FontTileSize*2, buffer);
}

static void renderGameTime(const Framebuffer &framebuffer)
{
    char buffer[64];

    sprintf(buffer, "%d", int(global.matchTime));
    drawText(framebuffer, 0xFFFFFFFF, (framebuffer.width - FontTileSize*strlen(buffer))/2, 0, buffer);
}
static void renderPostProcess(const Framebuffer &framebuffer)
{
    if(!global.isPaused && !global.isGameCompleted)
        return;

    uint32_t colorMask = 0xFF808080;

    auto destRow = framebuffer.pixels;
    for(int y = 0; y < framebuffer.height; ++y, destRow += framebuffer.pitch)
    {
        auto dest = reinterpret_cast<uint32_t*> (destRow);
        for(int x = 0; x < framebuffer.width; ++x, ++dest)
        {
            if((x ^ y) & 1)
                *dest &= colorMask;
        }
    }
}
static void renderMessage(const Framebuffer &framebuffer)
{
    const char *message = nullptr;
    uint32_t color = -1;

    if(global.isGameCompleted)
    {
        message = "Congratulations!\0You have escaped\0the Earth\0 \0Welcome to Hell!!!\0 \0Press R to reset\0";
        color = 0xff00FFFF;
    }
    else if(!global.player.isAlive())
    {
        message = "Game Over\0Press R to reset\0";
        color = 0xff000080;
    }
    else if(global.isPaused)
    {
        message = "Paused\0";
        color = 0xffff0000;
    }

    if(!message)
        return;
    int lineCount = 0;
    int lineStart = 0;
    while(message[lineStart])
    {
        lineStart += strlen(message + lineStart) + 1;
        ++lineCount;
    }

    int y = (framebuffer.height - 1 - FontTileSize*lineCount) / 2;
    lineStart = 0;
    while(message[lineStart])
    {
        int length = strlen(message + lineStart);
        int width = length* FontTileSize;

        int x = (framebuffer.width - width) / 2;
        drawText(framebuffer, color, x, y, message + lineStart);

        lineStart += length + 1;
        y += FontTileSize;
    }
}

static void renderHud(const Framebuffer &framebuffer)
{
    renderMinimap(framebuffer);
    renderHealth(framebuffer);
    renderBelly(framebuffer);
    renderAmmo(framebuffer);
    renderGameTime(framebuffer);

    renderMessage(framebuffer);
}

void render(const Framebuffer &framebuffer)
{
    renderBackground(framebuffer);
    renderEntities(framebuffer);
    renderBullets(framebuffer);
    renderPostProcess(framebuffer);
    renderHud(framebuffer);
}

Box2 getScreenBoundingBox()
{
    Vector2 halfExtent = pixels2Units(Vector2(ScreenWidth/2, ScreenHeight/2));

    return Box2(-halfExtent, halfExtent);
}

Box2 getScreenWorldBoundingBox()
{
    return getScreenBoundingBox().translatedBy(global.camera.position);
}
