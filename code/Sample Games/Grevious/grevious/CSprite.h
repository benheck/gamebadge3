#ifndef CSPRITE_H
#define CSPRITE_H
#pragma once

#include <stdio.h>

class CSprite {
  private:
    short X_Loc, Y_Loc;
    int8_t X_Dir, Y_Dir;
    int8_t X_Spd, Y_Spd;
    uint8_t Tile_X, Tile_Y;
    uint8_t Tile_sz;
    uint8_t Palette_Num;
    bool bAlive;
    void Init();

  public:
    CSprite(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
    ~CSprite();

    // Public Vars
    uint8_t yDrawOffset = 0;

    // Work Methods
    void BitBlit();
    bool IsCollided(CSprite *);

    // Accessor Methods
    bool IsAlive() { return bAlive; };
    void SetAlive(bool bNew) { bAlive = bNew; };
    short GetX() { return X_Loc; };
    void SetX(int vNew) { X_Loc = vNew; };
    void IncX(int vNew) { X_Loc += vNew; };
    short GetY() { return Y_Loc; };
    void SetY(int vNew) { Y_Loc = vNew; };
    void IncY(int vNew) { Y_Loc += vNew; };
    int8_t GetXDir() { return X_Dir; };
    void SetXDir(int vNew) { X_Dir = vNew; };
    int8_t GetYDir() { return Y_Dir; };
    void SetYDir(int vNew) { Y_Dir = vNew; };
    int8_t GetXSpeed() { return X_Spd; };
    void SetXSpeed(int vNew) { X_Spd = vNew; };
    int8_t GetYSpeed() { return Y_Spd; };
    void SetYSpeed(int vNew) { Y_Spd = vNew; };
    uint8_t GetTileX() { return Tile_X; };
    uint8_t GetTileY() { return Tile_Y; };
    uint8_t GetTileSize() { return Tile_sz; }; 
    uint8_t GetPaletteNum() { return Palette_Num; };
    void SetPalette(int vNew) { Palette_Num = vNew; };
};

#endif
