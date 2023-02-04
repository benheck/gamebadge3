#include "CSprite.h"
#include <gameBadgePico.h>

CSprite::CSprite(uint8_t TileX, uint8_t TileY, uint8_t TileSize, uint8_t Palette=0, uint8_t YDrawOffset=0)
{
    Tile_X = TileX;
    Tile_Y = TileY;
    Tile_sz = TileSize;
    Palette_Num = Palette;
    x = 0;
    y = 0;
    X_Dir = 1;
    Y_Dir = 1;
    X_Spd = 1;
    Y_Spd = 1;
    yDrawOffset = YDrawOffset;
    bAlive = false;
}
void CSprite::SetHitBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    hbX = x;
    hbY = y;
    hbW = w;
    hbH = h;
}

void CSprite::SetAnimation(int frames, int speed)
{
    num_frames = frames;
    anim_speed = speed;
}

void CSprite::BitBlit()
{
    drawSprite(x, y + yDrawOffset, GetTileX() + curr_frame * Tile_sz, GetTileY(), GetTileSize(), GetTileSize(), GetPaletteNum(), false, false);
}

bool CSprite::IsCollided(CSprite *s)
{
    bool xHit = false;
    bool yHit = false;

    if (x + hbX + hbW > s->x + s->hbX && x + hbX < s->x + s->hbX + s->hbW) xHit = true;
    
    if (y + hbY + hbH > s->y + s->hbY && y + hbY < s->y + s->hbY + s->hbH) yHit = true;
    
    return xHit && yHit;
}

bool CSprite::IsVisible(int viewport_x)
{
    if (viewport_x + 119 >= xMap && viewport_x < xMap + szWidth)
        return true;

    return false;
}

void CSprite::NextFrame()
{
    frame_counter++;
    if (frame_counter == anim_speed)
    {
        frame_counter = 0;
        curr_frame++;
        if (curr_frame == num_frames)
            curr_frame = 0;
    }
}

CSprite::~CSprite()
{
}