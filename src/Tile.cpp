#include "Tile.hpp"
#include "Image.hpp"
#include "Renderer.hpp"
#include "GameLogic.hpp"
#include <assert.h>
#include <string.h>
#include <map>

static std::map<uint32_t, TileType> tileColorTypeMap;
uint32_t tileColorPalette[256];

static struct TileColors
{
    TileColors()
    {
        setColor(TileType::DeepWater,       0xff000080);
        setColor(TileType::Water,           0xff0000ff);
        setColor(TileType::ShallowWater,    0xff00ffff);
        setColor(TileType::Ice,             0xff80ffff);
        setColor(TileType::Sand,            0xffffff00);
        setColor(TileType::Grass,           0xff00ff00);
        setColor(TileType::Earth,           0xff804000);
        setColor(TileType::Rock,            0xff404040);
        setColor(TileType::Forest,          0xff008000);
    }

    void setColor(TileType type, uint32_t rgbaColor)
    {
        auto color = convertRGBAHex(rgbaColor);
        tileColorPalette[(int)type] = color;
        tileColorTypeMap[color] = type;
    }
} tileColorsClass;

void TileMap::loadFromFile(const char *fileName)
{
    Image image;
    image.load(fileName);
    assert(image.width == Width);
    assert(image.height == Height);
    assert(image.bpp == 32);

    auto source = image.data + (image.height - 1)*image.pitch;
    size_t destRow = 0;
    memset(tiles, 0, sizeof(tiles));
    for(int y = 0; y < Height; ++y, source -= image.pitch, destRow += Width)
    {
        auto sourceRow = reinterpret_cast<uint32_t *> (source);
        auto destIndex = destRow;
        for(int x = 0; x < Width; ++x, ++destIndex)
        {
            auto color = sourceRow[x];
            auto type = tileColorTypeMap[color];
            if(type == TileType::None)
                printf("Unidentified color %08x\n", color);
            tiles[destIndex] = type;
            tileRandom[destIndex] = global.random.next32();
        }
    }

    image.destroy();

    postProcess();
}

void TileMap::postProcess()
{
    Coordinate row(tiles, Width, Height);

    for(int y = 0; y < Height; ++y, row.advanceRow())
    {
        Coordinate position = row;
        for(int x = 0; x < Width; ++x, position.advanceColumn())
        {
            auto &center = position.value();
            if(!isTileTypeInSet(center, TileTypeMask::OceanWater))
                continue;

            auto topLeft = position.atDeltaWrap(-1, 1); auto top = position.atDeltaWrap(0, 1); auto topRight = position.atDeltaWrap(1, 1);
            auto left = position.atDeltaWrap(-1, 0); /*auto center = position.atDeltaWrap(0, 0);*/ auto right = position.atDeltaWrap(1, 0);
            auto bottomLeft = position.atDeltaWrap(-1, -1); auto bottom = position.atDeltaWrap(0, -1); auto bottomRight = position.atDeltaWrap(1, -1);

            if(isTileTypeInSet(topLeft, TileTypeMask::AnyGround) || isTileTypeInSet(top, TileTypeMask::AnyGround) || isTileTypeInSet(topRight, TileTypeMask::AnyGround) ||
                isTileTypeInSet(left, TileTypeMask::AnyGround) || isTileTypeInSet(right, TileTypeMask::AnyGround) ||
                isTileTypeInSet(bottomLeft, TileTypeMask::AnyGround) || isTileTypeInSet(bottom, TileTypeMask::AnyGround) || isTileTypeInSet(bottomRight, TileTypeMask::AnyGround))
            {
                center = TileType::ShallowWater;
            }
        }
    }
}
