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
        setColor(TileType::DevilStone,      0xff000000);
        setColor(TileType::HolyBarrier,     0xffffffff);
    }

    void setColor(TileType type, uint32_t rgbaColor)
    {
        auto color = convertRGBAHex(rgbaColor);
        tileColorPalette[(int)type] = color;
        tileColorTypeMap[color] = type;
    }
} tileColorsClass;

static float TileOccupantProbabilities[(int)TileOccupant::Count] = {
    /* None */ 40000,

    // Nasty stuff
    /*Turret*/ 400,

    // Items
    /* Apple */ 200,
    /* Meat */ 250,
    /* MilitaryMeal*/ 80,
    /* Medkit */ 100,
    /* Bullet */250,
    /* TripleBullet */50,
    /* DemolitionBullet */25,
    /* TripleDemolitionBullet */5,
};

static const uint32_t TileOccupantPermittedTileMask[(int)TileOccupant::Count] = {
    /* None */ ~0u,

    // Nasty stuff
    /*Turret*/ TileTypeMask::Anywhere,

    // Items
    /* Apple */ TileTypeMask::Grass | TileTypeMask::Forest | TileTypeMask::Earth,
    /* Meat */ TileTypeMask::AnyGround,
    /* MilitaryMeal*/ TileTypeMask::Anywhere,
    /* Medkit */ TileTypeMask::Anywhere,
    /* Bullet */TileTypeMask::Anywhere,
    /* TripleBullet */TileTypeMask::Anywhere,
    /* DemolitionBullet */TileTypeMask::Anywhere,
    /* TripleDemolitionBullet */TileTypeMask::Anywhere,

};

static TileOccupant tileOccupantProbabilityDistribution[1<<16];

static struct TileOccupantProbabilityComputer
{
    TileOccupantProbabilityComputer()
    {
        constexpr int TableSize = 1<<16;

        int numberOfElements = int(TileOccupant::Count);
        float probabilitySum = 0;
        for(int i = 0; i < numberOfElements; ++i)
            probabilitySum += TileOccupantProbabilities[i];

        int destIndex = 0;
        for(int i = 0; i < numberOfElements; ++i)
        {
            int elementCount = TableSize * TileOccupantProbabilities[i] / probabilitySum;
            int nextIndex = destIndex + elementCount;
            if(nextIndex > TableSize)
                nextIndex = TableSize;

            for(int d = destIndex; d < nextIndex; ++d)
                tileOccupantProbabilityDistribution[d] = TileOccupant(i);
            destIndex = nextIndex;
        }

        for(int d = destIndex; d < TableSize; ++d)
            tileOccupantProbabilityDistribution[d] = TileOccupant::None;
    }

} tileOccupantProbabilityComputer;

inline TileOccupant generateTileOccupant(TileType type, int row, int column)
{
    constexpr Box2 NoDemolitionZone = Box2(137, 47, 208, 147);
    auto point = Vector2(column, row);

    for(;;)
    {
        auto occupant = tileOccupantProbabilityDistribution[global.random.next32() & 0xFFFF];
        if(!isTileTypeInSet(type, TileOccupantPermittedTileMask[int(occupant)]))
            continue;

        // No nukes in south america.
        if((occupant == TileOccupant::DemolitionBullet || occupant == TileOccupant::TripleDemolitionBullet) && NoDemolitionZone.containsPoint(point))
            continue;

        return occupant;
    }

    return TileOccupant::None;
}

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

            auto tileOccupant = generateTileOccupant(type, y, x);
            occupants[destIndex] = tileOccupant;
            occupantStates[destIndex].setDefault(tileOccupant);
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
            if(!isTileTypeInSet(center, TileTypeMask::Water))
                continue;

            auto topLeft = position.atDeltaWrap(-1, 1); auto top = position.atDeltaWrap(0, 1); auto topRight = position.atDeltaWrap(1, 1);
            auto left = position.atDeltaWrap(-1, 0); /*auto center = position.atDeltaWrap(0, 0);*/ auto right = position.atDeltaWrap(1, 0);
            auto bottomLeft = position.atDeltaWrap(-1, -1); auto bottom = position.atDeltaWrap(0, -1); auto bottomRight = position.atDeltaWrap(1, -1);

            if( isTileTypeInSet(topLeft, TileTypeMask::NonWater)    || isTileTypeInSet(top, TileTypeMask::NonWater)     || isTileTypeInSet(topRight, TileTypeMask::NonWater) ||
                isTileTypeInSet(left, TileTypeMask::NonWater)       || isTileTypeInSet(right, TileTypeMask::NonWater)   ||
                isTileTypeInSet(bottomLeft, TileTypeMask::NonWater) || isTileTypeInSet(bottom, TileTypeMask::NonWater)  || isTileTypeInSet(bottomRight, TileTypeMask::NonWater))
            {
                center = TileType::ShallowWater;
            }
        }
    }
}
