#include <gameBadgePico.h>
#include "CSprite.h"
#include "Enums.h"
#include "GameLibrary.h"
#include "LevelData.h"
#include "structs.h"

#define BULLET_X_OFFSET     8
#define BULLET_Y_OFFSET     1
#define SCREEN_W            120
#define SCREEN_H            120
#define NUM_TILE_COLS       15
#define NUM_TILE_ROWS       14
#define SHIP_W              16
#define SHIP_H              16
#define STATUS_BAR_H        8
#define STATBAR_NT_ROW      17
#define Y_OFFSET            8
#define NUM_BULLETS         7
#define VIEWPORT_W          NUM_TILE_COLS * 8

#define NOTHING             0
#define EXPLODING           1
#define WAITING             2

struct repeating_timer timer30Hz;			//This runs the game clock at 30Hz
bool paused = false;						      //Player pause, also you can set pause=true to pause the display while loading files
bool nextFrameFlag = false;					  //The 30Hz ISR sets this flag, Core0 Loop responds to it
bool drawFrameFlag = false;					  //When Core0 Loop responses to nextFrameFlag, it sets this flag so Core1 will know to render a frame
bool frameDrawing = false;					  //Set TRUE when Core1 is drawing the display. Core0 should wait for this to clear before accessing video memory

uint8_t screenX = 0;
uint16_t curMapPage = 0;
int mapScrollX = 0;
bool scrolling_paused = true;
unsigned long ticks = 0;

const size_t mapWidth = sizeof L01_TILES / NUM_TILE_ROWS;
const uint16_t lastMapPage = mapWidth / NUM_TILE_COLS - 1;
const uint8_t paletteMap[] = {
    0, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 
    0, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

CSprite *csShip, *csShipExplosion;
CSprite *csBullet[NUM_BULLETS];
CSprite *csMobs[L01_MOBS_NUM];
enemy mobs[L01_MOBS_NUM];
missle shots[NUM_BULLETS];

// Miscellaneous Variables
GameState gameState = GS_TITLE;
player p1;
int bulletIdx = 0;

void setup() {
    paused = true;                      //Prevent 2nd core from drawing the screen until file system is booted

    gamebadge3init();

    if (loadRGB("grevious.nes.pal")) {
        loadPalette("grevious.nes.dat");

        if (gameState == GS_TITLE) { 
            loadPattern("title.nes", 0, 512);
        } else {
            loadPattern("grevious.nes", 0, 512);
            LoadTilePage(0, 0);
        }

        paused = false;
    }

    // Set up the level
    ResetGame();

    // Set up sprites
    csShip = new CSprite(0, 16, 2, 3, Y_OFFSET);            // Player's ship
    csShip->SetHitBox(1, 5, 13, 7);

    csShipExplosion = new CSprite(6, 16, 2, 5, Y_OFFSET);
    csShipExplosion->SetAnimation(4, 8);

    // Load the bullets
    for (uint8_t n = 0; n < NUM_BULLETS; n++) {
        csBullet[n] = new CSprite(6, 16, 1, 1, Y_OFFSET);
        csBullet[n]->SetHitBox(1, 2, 3, 6);
    }
    
    // Load the mob sprites
    for (int n = 0; n < L01_MOBS_NUM; n++)
        switch (mobs[n].type) {
            case CANNON:
                csMobs[n] = new CSprite(0, 23, 2, 1, Y_OFFSET);
                csMobs[n]->SetHitBox(4, 3, 10, 13);
                break;
            case UFO:
                csMobs[n] = new CSprite(0, 21, 2, 1, Y_OFFSET);
                csMobs[n]->SetHitBox(2, 4, 12, 8);
                break;
            case ASTEROID:
                csMobs[n] = new CSprite(0, 19, 2, 6, Y_OFFSET);
                csMobs[n]->SetHitBox(3, 2, 10, 12);
                csMobs[n]->SetAnimation(4, 5);
                break;
            default:
                break;
        }
}

void setup1() {
    clearSprite();

    // Freeze the first row to use as a status bar
    setWinYjump(0, STATBAR_NT_ROW);                   // Display the tiles at row 17 in the name table in place of row 0 on the LCD

    add_repeating_timer_ms(33, timer_isr, NULL, &timer30Hz);    // Start the timer for updating the display
}

void loop() {
    if (nextFrameFlag == true) {		  //Flag from the 30Hz ISR?
        nextFrameFlag = false;			  //Clear that flag		
        drawFrameFlag = true;			    //and set the flag that tells Core1 to draw the LCD
        ticks++;

        switch (gameState)
        {
        case GS_GAME:
            GameLoop();					        //Now do our game logic in Core0
            break;
        case GS_TITLE:
            TitleLoop();
            break;
        case GS_GAMEOVER:
            GameOverLoop();
            break;
        case GS_DEATH:
            DeathLoop();
            break;
        default:
            break;
        }

        serviceDebounce();            // Debounce buttons - must be called every frame
    }
}

void loop1() {
    // If flag set to draw display
    if (drawFrameFlag == true) {			
        drawFrameFlag = false;				//Clear flag
        
        if (paused == true) {				  //If system paused, return (no draw)
            return;
        }

        frameDrawing = true;				  //Set flag that we're drawing a frame. Core0 should avoid accessing video memory if this flag is TRUE
        sendFrame();
        frameDrawing = false;				  //Clear flag. Core0 is now free to access screen memory (I guess this core could as well?)
    }
}

//--------------------This is called at 30Hz. Your main game state machine should reside here

void GameLoop() { 
    // 1. Erase Objects (i.e. text)
    //------------------------------

    // 2. Get Input
    //--------------
        while(frameDrawing == false) delayMicroseconds(1);      // Wait for Core1 to begin rendering the frame
        HandleButtons();                                        
        while(frameDrawing == true) delayMicroseconds(1);       // Wait for Core 1 to finish drawing

    // 3. Move Objects
    //-----------------
        setWindow(screenX, 0);
        setWindowSlice(STATBAR_NT_ROW, 0);                      // Freeze Scroll of the status bar
        setSpriteWindow(0, STATUS_BAR_H, 119, 119);             //Set window in which sprites can appear

        MoveObjects();

    // 4. Do Collisions
    //------------------
        if (p1.state == ALIVE)
            for (uint16_t n = 0; n < L01_MOBS_NUM; n++)
                if (csMobs[n]->IsVisible(mapScrollX) && mobs[n].state == ALIVE)
                {
                    if (csShip->IsCollided(csMobs[n]))
                    {
                        gameState = GS_DEATH;
                    }
                    
                    // Check for bullet hit and kill mob
                    for (int m = 0; m < NUM_BULLETS; m++)
                        if (shots[m].state == ACTIVE)
                            if (csMobs[n]->IsCollided(csBullet[m])) {
                                mobs[n].hp--;                         
                                shots[m].state = SPENT;
                                if (mobs[n].hp <= 0)
                                {
                                    mobs[n].state = DEAD;
                                    p1.score += mobs[n].points;
                                }
                            }
                }
    
    // 5. Scan background
    //--------------------
        if (IsWallHit())
            gameState = GS_DEATH;

    // 6. Draw Objects
    //-----------------
        DrawObjects();

        // Update Status Bar
        char strLives[2], strScore[8];
        sprintf(strScore, "%07d", p1.score);
        itoa(p1.lives, strLives, 10);
        drawText(strScore, 0, STATBAR_NT_ROW, false);
        drawText(strLives, 14, STATBAR_NT_ROW, false);
}

void TitleLoop() {
    while(frameDrawing == false) delayMicroseconds(1);

    while(frameDrawing == true) delayMicroseconds(1);

    if (button(A_but)) {
        loadPattern("grevious.nes", 0, 512);
        LoadTilePage(0, 0);                         // Load the first screen of tiles
        ResetGame();
        gameState = GS_GAME;
        return;
    }

    setWindow(0, 0);
    setSpriteWindow(0, 0, 119, 119);
    drawSprite(0, 0, 0, 16, 15, 15, 1, false, false);
}

void GameOverLoop() {
    while(frameDrawing == false) delayMicroseconds(1);
    while(frameDrawing == true) delayMicroseconds(1);

    drawText("GAME OVER", 3 + (screenX >> 3), 7, false);

    if (button(A_but)) {
        curMapPage = 0;
        screenX = 0;
        mapScrollX = 0;
        LoadTilePage(0, 0);
        ResetGame();
        gameState = GS_GAME;
    }
}

void DeathLoop() {
    while(frameDrawing == false) delayMicroseconds(1);
    while(frameDrawing == true) delayMicroseconds(1);

    static int death_loop_state = EXPLODING;                                    // Represents where we're at in the death loop
    static int tick_count = 90;                                                 // Number of frames to wait before restarting level

    if (death_loop_state == EXPLODING)                                          // Play the explosion animation
    {
        csShipExplosion->x = p1.x;                                              // Explosion should happen in the spot the ship was
        csShipExplosion->y = p1.y;
        csShipExplosion->BitBlit();
        if (csShipExplosion->curr_frame == csShipExplosion->num_frames - 1)     // Explosion animation finished
        {
            csShipExplosion->curr_frame = 0;                                    // Reset the animation
            death_loop_state = WAITING;                                         // Move to waiting phase
        }
        else csShipExplosion->NextFrame();                                      // Continue animation
    }
    
    if (death_loop_state == WAITING)                                            // Wait before restarting the level
    {
        if (tick_count > 0) tick_count--;
        else                                                                    // We're done waiting
        {
            death_loop_state = EXPLODING;                                       // Reset the death loop state
            tick_count = 90;                                                    // Reset the counter
            KillPlayer();                                                       // Kill the player
        }
    }
}

//Every 33ms, set the flag that it's time to draw a new frame (it's possible system might miss one but that's OK)
bool timer_isr(struct repeating_timer *t) {				
    nextFrameFlag = true;
    return true;
}

/*************************************************************************
* ResetGame
* Initializes all of the variables used in the game
*************************************************************************/
void ResetGame()
{
    // set up the player's ship
    p1.lives = 3;
    p1.score = 0;
    
    ResetLevel();
}

/*************************************************************************
* ResetLevel
* Re-initializes all of the variables used in the current level
*************************************************************************/
void ResetLevel()
{
    // set up the player's ship
    p1.x = 40;
    p1.y = 56;
    p1.speed_x = 1;
    p1.speed_y = 1;
    p1.bullet_index = 0;
    p1.state = ALIVE;

    csShip->SetX(p1.x);
    csShip->SetY(p1.y);

    // set up the bullets
    for (int n = 0; n < NUM_BULLETS; n++) 
    {
        shots[n].x = 0;
        shots[n].y = 0;
        shots[n].speed_x = 3;
        shots[n].state = SPENT;
    }

    // Reset the mobs
    for (uint16_t n = 0; n < L01_MOBS_NUM; n++)
    {
        mobs[n] = L01_MOBS[n];
        csMobs[n]->xMap = mobs[n].x;
        csMobs[n]->yMap = mobs[n].y;
        csMobs[n]->SetX(csMobs[n]->xMap);
        csMobs[n]->SetY(csMobs[n]->yMap);
    }
    
    scrolling_paused = false;
}

/*************************************************************************
* HandleButtons
* Checks and handles button events
*************************************************************************/
void HandleButtons()
{
    if (button(right_but)) {
        p1.x += p1.speed_x;
        if (p1.x > SCREEN_W) p1.x = SCREEN_W;
        csShip->SetX(p1.x);
    }

    if (button(left_but)) {
        p1.x -= p1.speed_x;
        if (p1.x < 0) p1.x = 0;
        csShip->SetX(p1.x);
    }

    if (button(up_but)) {
        csShip->curr_frame = 2;
        p1.y -= p1.speed_y;
        if (p1.y < 0) p1.y = 0;
        csShip->SetY(p1.y);
    }

    if (button(down_but)) {
        csShip->curr_frame = 1;
        p1.y += p1.speed_y;
        if (p1.y + SHIP_H > SCREEN_H - STATUS_BAR_H) p1.y = SCREEN_H - STATUS_BAR_H - SHIP_H;
        csShip->SetY(p1.y);
    }

    if (button(A_but)) {
        p1.bullet_index++;
        if (p1.bullet_index > NUM_BULLETS - 1) p1.bullet_index = 0;

        if (shots[p1.bullet_index].state == SPENT) 
        {
            shots[p1.bullet_index].x = p1.x + BULLET_X_OFFSET;
            shots[p1.bullet_index].y = p1.y + BULLET_Y_OFFSET;
            shots[p1.bullet_index].state = ACTIVE;
        }    
    }
}

/*************************************************************************
* MoveObjects
* Main Animation Routine
*************************************************************************/
void MoveObjects() {
    short n;                                    // A counter that we can reuse

    // Move the player's bullets
    for (n = 0; n < NUM_BULLETS; n++) {
        if (shots[n].state == ACTIVE) 
        {
            shots[n].x += shots[n].speed_x;
            if (shots[n].x > 128)
                shots[n].state = SPENT;
        }
    }

    // Update mob positions
    for (n = 0; n < L01_MOBS_NUM; n++) {
        if (csMobs[n]->IsVisible(mapScrollX) && mobs[n].state == ALIVE)
        {
            mobs[n].x -= mobs[n].speed_x;
            csMobs[n]->SetX(mobs[n].x - mapScrollX);
        }
    }
    
    // Scroll the screen
    if (!scrolling_paused) {
        if (screenX == 0) {                             // If viewport is at x=0, buffer the next map page
            if (curMapPage != lastMapPage)              // Make sure we're not on the last page in the level map
                LoadTilePage(curMapPage + 1, 1);        // Buffer the next map page into upper right of the name table
            else 
                scrolling_paused = true;                // We're at the end of the map, so stop scrolling (and queue boss fight!)
        }

        screenX++;                                      // Advance the viewport
        mapScrollX++;                                   // Advance the counter that keeps track of where we are on the level map
        if (screenX == SCREEN_W) {                      // X = 120 indicates that we've scrolled past one full screen...
            curMapPage++;                               // Increment tile map screen counter
            LoadTilePage(curMapPage, 0);                // Buffer the map page we're currently on into upper left of the name table
            screenX = 0;                                // Reset viewport x to 0
        } 
    }
}

/*************************************************************************
* LoadTilePage
* Loads a screen of tiles onto the name table
*************************************************************************/
void LoadTilePage(uint16_t srcPage, uint8_t dstPage)
{
    for (uint8_t x = 0; x < NUM_TILE_COLS; x++) 
        for (uint8_t y = 0; y < NUM_TILE_ROWS; y++) {
            uint8_t tileIdx = L01_TILES[x+srcPage*NUM_TILE_COLS+y*mapWidth];
            drawTile(x+dstPage*NUM_TILE_COLS, y, tileIdx, paletteMap[tileIdx]);
        }
}

/*************************************************************************
* IsWallHit
* Checks the on-screen wall tiles to determine if the player hit the wall
*************************************************************************/
bool IsWallHit()
{
    // Define the ship's bounding box. We're going to check for wall hits at every point in the box
    uint8_t x1 = p1.x + 1;
    uint8_t x2 = x1 + SHIP_W - 3;
    uint8_t y1 = p1.y + 5;
    uint8_t y2 = y1 + SHIP_H - 4;

    // Iterate through the bounding box and see if we hit a wall
    for (uint8_t m = x1; m <= x2; m++)
        for (uint8_t n = y1; n <= y2; n++) {
            uint8_t tileType = GetTileAtPoint(m, n);
            if (tileType > 0 && tileType < 9)
                return true;
        }
    
    return false;         // No wall hit
}



/*************************************************************************
* GetTileAtPoint
* Takes an x, y of a point on the screen and returns the tile type at
* that point.
*************************************************************************/
uint8_t GetTileAtPoint(uint8_t x, uint8_t y) {
    uint8_t tileOffsetX = mapScrollX >> 3;
    uint8_t xTileIndex = x >> 3;
    uint8_t yTileIndex = y >> 3;
    return L01_TILES[(xTileIndex + tileOffsetX) + mapWidth * yTileIndex];
}

/*************************************************************************
* KillPlayer
* Post-death routine
*************************************************************************/
void KillPlayer() {
    p1.lives--;
    if (p1.lives == 0)
    {
        gameState = GS_GAMEOVER;
        return;
    } else {
        gameState = GS_GAME;
    }

    curMapPage = 0;
    screenX = 0;
    mapScrollX = 0;
    LoadTilePage(0, 0);
    ResetLevel();
}

/*************************************************************************
* DrawObjects
* Checks if sprites are in the viewport and draws them
*************************************************************************/
void DrawObjects() {
    if (p1.state == ALIVE)
    {
        csShip->BitBlit();
        csShip->curr_frame = 0;
    }         

    // Draw the bullets
    for (uint8_t n = 0; n < NUM_BULLETS; n++)
        if (shots[n].state == ACTIVE)
        {
            csBullet[n]->x = shots[n].x;
            csBullet[n]->y = shots[n].y;
            csBullet[n]->BitBlit();
        }
            
    // Draw the mobs
    for (uint16_t n = 0; n < L01_MOBS_NUM; n++)
    {
        if (csMobs[n]->IsVisible(mapScrollX) && mobs[n].state == ALIVE)
        {
            csMobs[n]->BitBlit();
            csMobs[n]->NextFrame();
        }
    }
}