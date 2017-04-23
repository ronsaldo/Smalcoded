#include "Renderer.hpp"
#include "GameLogic.hpp"
#include <math.h>
#include <algorithm>
#include <stdio.h>

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

static void drawBox(const Framebuffer &framebuffer, uint32_t color, const Box2 &box)
{
    auto min = viewToScreen(framebuffer, box.min).floor();
    auto max = viewToScreen(framebuffer, box.max).floor();

    drawRectangle(framebuffer, color, min.x, min.y, max.x - min.x, max.y - min.y);
}

static void renderBackground(const Framebuffer &framebuffer)
{
    auto minPosition = screenToWorld(framebuffer, 0, 0).floor();
    auto maxPosition = screenToWorld(framebuffer, framebuffer.width, framebuffer.height).floor();

    auto offset = worldToScreen(framebuffer, minPosition);
    int offsetX = offset.x;
    int offsetY = offset.y;
    int pixelsPerTile = Units2Pixels;

    int minX = minPosition.x;
    int minY = minPosition.y;
    int maxX = maxPosition.x;
    int maxY = maxPosition.y;

    auto destY = offsetY;
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
            blitTileRectangle(framebuffer, destX, framebuffer.height - destY - pixelsPerTile, global.mapTileSet, global.mapTileSet.getTileRectangle(int(tileType), animationVariant, pixelsPerTile, pixelsPerTile));
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
        blitTileRectangle(framebuffer, destX, destY, global.mapTileSet, global.mapTileSet.getTileRectangle(entity.spriteRow, entity.spriteColumn, 32, 32),
        entity.flipHorizontal, entity.flipVertical);
        break;
    case SpriteType::Character:
        blitTileRectangle(framebuffer, destX, destY, global.characterTileSet, global.characterTileSet.getTileRectangle(entity.spriteRow, entity.spriteColumn, 32, 48),
        entity.flipHorizontal, entity.flipVertical);
        break;
    case SpriteType::None:
    default:
    //drawBox(framebuffer, encodeColor(255, 255, 255, 255), entity.boundingBox.translatedBy(translation));
        drawBox(framebuffer, encodeColor(255, 255, 255, 255), entity.boundingBox.translatedBy(worldToView(entity.position)));
        break;
    }
}

static void renderEntities(const Framebuffer &framebuffer)
{
    renderEntity(framebuffer, global.player);
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
    drawRectangle(framebuffer, 0xFFFFFFFF, cursorX - cursorWidth/2, cursorY - cursorHeight/2, cursorWidth, cursorHeight);
}

static void renderHud(const Framebuffer &framebuffer)
{
    renderMinimap(framebuffer);
}

void render(const Framebuffer &framebuffer)
{
    renderBackground(framebuffer);
    renderEntities(framebuffer);
    renderHud(framebuffer);
}
