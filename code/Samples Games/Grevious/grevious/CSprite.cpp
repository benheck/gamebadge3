#include "CSprite.h"
#include <gameBadgePico.h>

CSprite::CSprite(uint8_t TileX, uint8_t TileY, uint8_t TileSize, uint8_t Palette=0)
{
    Tile_X = TileX;
    Tile_Y = TileY;
    Tile_sz = TileSize;
    Palette_Num = Palette;
    X_Loc = 0;
    Y_Loc = 0;
    X_Dir = 1;
    Y_Dir = 1;
    X_Spd = 1;
    Y_Spd = 1;
    bAlive = false;
}

void CSprite::BitBlit()
{
    drawSprite(GetX(), GetY(), GetTileX(), GetTileY(), GetTileSize(), GetTileSize(), GetPaletteNum());
}

bool CSprite::IsCollided(CSprite *csSource)
{
    if (X_Loc < csSource->GetX() + 8 && X_Loc + 8 > csSource->GetX() && Y_Loc < csSource->GetY() + 8 && Y_Loc + 8 > csSource->GetY())
    {
        return true;
    }
    return false;
}

CSprite::~CSprite()
{
}