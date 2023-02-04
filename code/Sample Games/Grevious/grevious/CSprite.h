#ifndef CSPRITE_H
#define CSPRITE_H
#pragma once

#include <stdio.h>

class CSprite {
  private:
    int8_t X_Dir, Y_Dir;
    int8_t X_Spd, Y_Spd;
    uint8_t Tile_X, Tile_Y;
    uint8_t Tile_sz;
    uint8_t Palette_Num;
    bool bAlive;
    void Init();
    int frame_counter = 0;                  // Internal counter used for keeping track of animation speed

  public:
    CSprite(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
    ~CSprite();

    // Public Vars
    short x, y;
    uint8_t hbX = 0;                        // Starting X of the sprite's hitbox (relative to the sprite)
    uint8_t hbY = 0;                        // Starting Y of the sprite's hitbox (relative to the sprite)
    uint8_t hbW = 16;                       // Width of the sprite's hitbox
    uint8_t hbH = 16;                       // Height of the sprite's hitbox
    uint8_t yDrawOffset = 0;
    uint16_t xMap = 0;
    uint16_t yMap = 0;
    uint8_t szWidth = GetTileSize() * 8;
    int curr_frame = 0;                     // The current frame of the sprite being drawn
    int num_frames = 1;                     // The total number of sprite frames
    int anim_speed = 4;                     // Speed of the animation - higher number = slower
    

    // Work Methods
    void BitBlit();
    bool IsCollided(CSprite *);
    void SetHitBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
    bool IsVisible(int viewport_x);
    void NextFrame();                             // Progresses to the next frame of the animation
    void SetAnimation(int frames, int speed);     // Configure the animation settings

    // Accessor Methods
    bool IsAlive() { return bAlive; };
    void SetAlive(bool bNew) { bAlive = bNew; };
    short GetX() { return x; };
    void SetX(int vNew) { x = vNew; };
    void IncX(int vNew) { x += vNew; };
    short GetY() { return y; };
    void SetY(int vNew) { y = vNew; };
    void IncY(int vNew) { y += vNew; };
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
