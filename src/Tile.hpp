#ifndef SMALL_ECO_DESTROYED_TILE_HPP
#define SMALL_ECO_DESTROYED_TILE_HPP

#include <stdint.h>
#include <stddef.h>
#include "Vector2.hpp"
#include "Rectangle.hpp"
#include "Image.hpp"
#include <assert.h>

enum class TileType: uint8_t
{
    None = 0,
    DeepWater,
    Water,
    ShallowWater,
    Ice,
    Sand,
    Grass,
    Forest,
    Earth,
    Rock,

    Count,
};

namespace TileTypeMask
{
enum Bits
{
    None = 1<<int(TileType::None),
    DeepWater = 1<<int(TileType::DeepWater),
    Water = 1<<int(TileType::Water),
    ShallowWater = 1<<int(TileType::ShallowWater),
    Ice = 1<<int(TileType::Ice),
    Sand = 1<<int(TileType::Sand),
    Grass = 1<<int(TileType::Grass),
    Forest = 1<<int(TileType::Forest),
    Earth = 1<<int(TileType::Earth),
    Rock = 1<<int(TileType::Rock),

    AnyWater = DeepWater | Water | ShallowWater,
    OceanWater = DeepWater | Water,
    AnyGround = Ice | Sand | Grass | Forest | Earth,
};
};

inline bool isTileTypeInSet(TileType type, uint32_t set)
{
    return (set & (1<<int(type))) != 0;
}

extern uint32_t tileColorPalette[256];
static constexpr int WorldWidth = 512;
static constexpr int WorldHeight = 256;

inline Vector2 normalizeWorldCoordinate(const Vector2 &point)
{
    return Vector2(floorModule(point.x, float(WorldWidth)), floorModule(point.y, float(WorldHeight)));
}

template<typename ET>
struct ImageCoordinate
{
    typedef ET ElementType;

    ImageCoordinate(ElementType *data, int width, int height, int x = 0, int y = 0)
        : data(data), width(width), height(height), x(x), y(y) {}

    void advanceRow()
    {
        ++y;
        index += width;
    }

    void advanceColumn()
    {
        ++x;
        ++index;
    }

    ElementType &value()
    {
        return data[index];
    }

    ElementType atDeltaOr(int dx, int dy, ElementType defaultValue = ElementType())
    {
        auto nx = x + dx;
        auto ny = y + dy;
        if(0 <= nx && nx < width &&
           0 <= ny && ny < height)
        {
            return data[index + dy*width + dx];
        }
        else
        {
            return defaultValue;
        }
    }

    ElementType atDeltaWrap(int dx, int dy)
    {
        return atDeltaOr(dx, dy);
    }

    ElementType *data;
    int width;
    int height;

    int x;
    int y;
    int index;
};

struct TileMap
{
    typedef ImageCoordinate<TileType> Coordinate;

    static constexpr int Width = 512;
    static constexpr int Height = 256;

    void loadFromFile(const char *fileName);
    void postProcess();

    size_t tileIndexAtRowColumn(size_t row, size_t column)
    {
        return row*Width + column;
    }

    size_t tileIndexAtPoint(const Vector2 &point)
    {
        auto normalizedPoint = normalizeWorldCoordinate(point);
        return tileIndexAtRowColumn(normalizedPoint.y, normalizedPoint.x);
    }

    int animationVariant;
    TileType tiles[Width*Height];
    uint32_t tileRandom[Width*Height];

};

template<int W, int H>
struct TileSetImage
{
    static constexpr int Width = W;
    static constexpr int Height = H;

    void loadFromFile(const char *fileName)
    {
        Image image;
        image.load(fileName);
        assert(image.width == Width);
        assert(image.height == Height);
        assert(image.bpp == 32);
        assert(image.pitch == Width*4);

        memcpy(data, image.data, sizeof(data));
        image.destroy();
    }

    Rectangle getTileRectangle(int row, int column, int tileWidth = Units2Pixels, int tileHeight = Units2Pixels)
    {
        return Rectangle(column*tileWidth, row * tileHeight, tileWidth, tileHeight);
    }

    Rectangle wholeRectangle() const
    {
        return Rectangle(0, 0, Width, Height);
    }

    uint32_t data[Width*Height];
};

using TileSet = TileSetImage<512,512>;
using MiniMapImage = TileSetImage<128, 64>;


#endif //SMALL_ECO_DESTROYED_TILE_HPP
