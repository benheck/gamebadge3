#include <gameBadgePico.h>
#include "CSprite.h"
#include "GameLibrary.h"

#define BULLET_X_OFFSET 8
#define BULLET_Y_OFFSET 4
#define SCREEN_W 120
#define SCREEN_H 120
#define NUM_TILE_COLS 15
#define NUM_TILE_ROWS 15

struct repeating_timer timer30Hz;			//This runs the game clock at 30Hz
bool paused = false;						      //Player pause, also you can set pause=true to pause the display while loading files
bool nextFrameFlag = false;					  //The 30Hz ISR sets this flag, Core0 Loop responds to it
bool drawFrameFlag = false;					  //When Core0 Loop responses to nextFrameFlag, it sets this flag so Core1 will know to render a frame
bool frameDrawing = false;					  //Set TRUE when Core1 is drawing the display. Core0 should wait for this to clear before accessing video memory

uint8_t screenX = 0;
uint16_t curMapPage = 0;
int mapScrollX = 0;
bool scrolling_paused = false;

// Map Data
const uint8_t tilemap[] = {
    0,0,19,0,0,0,1,2,3,2,0,0,0,0,2,3,0,0,0,5,0,0,0,0,2,3,0,0,0,0,2,3,0,0,6,7,6,0,0,0,7,0,0,0,4,0,0,0,3,0,0,7,0,0,0,0,0,0,0,7,0,5,0,0,2,3,0,0,6,7,0,0,0,0,3,3,0,0,0,0,0,7,0,0,0,0,0,2,0,0,0,2,2,7,3,5,3,0,0,0,6,3,0,0,6,5,0,0,0,2,2,0,0,5,0,0,0,0,0,0,0,0,0,0,2,3,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,1,2,3,4,0,0,1,2,0,0,2,3,2,4,0,0,1,2,3,4,0,0,1,0,0,7,0,0,2,0,0,5,0,4,0,1,0,5,0,1,0,0,4,2,1,0,5,7,0,0,0,0,0,4,0,0,1,0,0,0,0,0,3,4,0,0,1,0,0,2,0,0,0,2,2,0,4,0,1,3,2,0,0,6,0,4,0,2,0,3,3,0,2,0,0,0,0,3,2,0,0,6,0,5,0,0,0,0,3,5,3,0,0,0,0,0,0,2,0,2,5,0,0,0,2,0,0,0,6,0,0,0,0,6,7,0,0,1,2,0,
    18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,4,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,0,2,4,0,1,0,0,4,0,0,0,1,4,0,0,1,4,0,0,0,1,2,0,0,0,3,2,4,0,0,0,0,1,2,2,3,4,0,0,0,0,0,1,4,0,1,3,4,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,6,0,0,0,0,1,4,1,3,0,0,0,0,4,0,0,7,0,3,2,0,2,0,4,1,3,0,0,2,0,1,0,4,0,1,3,0,1,3,0,0,0,2,3,0,0,0,3,4,0,0,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,1,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,4,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,3,4,0,0,1,3,4,0,0,0,0,0,
    0,0,19,0,0,0,0,19,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,19,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,19,0,0,0,0,0,0,1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,19,0,0,17,17,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,19,0,0,0,0,0,0,17,0,0,0,0,17,0,0,18,0,0,0,0,0,0,19,0,0,0,0,17,0,0,19,0,
    0,0,0,0,0,17,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,19,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,19,0,0,0,0,18,0,0,0,0,0,0,0,19,0,0,0,19,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,19,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,18,0,0,0,0,0,0,0,0,0,19,0,0,0,0,0,18,0,0,0,0,19,0,0,0,19,0,0,0,18,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,19,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,19,0,0,0,0,0,0,0,0,19,0,0,0,0,0,0,0,0,0,0,18,0,0,17,0,0,
    0,18,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,19,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,17,0,0,0,0,0,0,19,0,0,0,0,0,0,0,19,0,0,0,0,0,17,0,0,0,0,0,0,0,19,0,0,17,0,0,0,0,0,0,0,19,0,0,0,0,0,0,0,0,0,19,0,0,0,0,0,0,0,0,18,0,0,18,0,0,0,0,0,0,0,
    0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,19,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,19,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,17,0,0,17,0,0,0,0,0,0,0,0,0,0,19,0,0,0,0,18,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,19,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,17,0,0,0,18,0,0,0,0,0,19,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,17,0,0,0,19,0,
    0,0,0,0,0,17,0,0,0,18,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,17,0,0,0,0,0,17,0,0,0,0,0,17,0,0,0,0,0,0,18,0,0,17,0,0,18,0,0,0,19,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    19,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,19,0,0,0,18,0,19,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,19,0,19,0,0,0,18,0,0,0,0,0,0,0,0,17,0,0,0,18,0,0,17,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,19,0,0,0,0,0,0,18,0,0,19,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,19,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,17,0,0,0,0,0,18,0,0,0,0,0,0,19,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
const size_t mapWidth = sizeof tilemap / 15;
const uint16_t lastMapPage = mapWidth / 15 - 1;
const uint8_t paletteMap[] = {
    0, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 0, 0, 0, 0, 0, 
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

CSprite *csShip;
CSprite *csBullet[5];

// Miscellaneous Variables
POINT ship;
int bulletIdx = 0;

void setup() {
    paused = true;                      //Prevent 2nd core from drawing the screen until file system is booted

    gamebadge3init();

    if (loadRGB("grevious.nes.pal")) {
        loadPalette("grevious.nes.dat");
        loadPattern("grevious.nes", 0, 512);

        paused = false;
    }

    // Set up sprites
    csShip = new CSprite(0, 0, 2, 3);           // Player's ship
    for (uint8_t n = 0; n < 5; n++)             // Bullets
        csBullet[n] = new CSprite(2, 0, 1, 1);

    ResetGame();
}

void setup1() {
    LoadTilePage(0, 0);                         // Load the first screen of tiles

    clearSprite();

    add_repeating_timer_ms(33, timer_isr, NULL, &timer30Hz);    // Start the timer for updating the display
}

void loop() {
    if (nextFrameFlag == true) {		  //Flag from the 30Hz ISR?
        nextFrameFlag = false;			  //Clear that flag		
        drawFrameFlag = true;			    //and set the flag that tells Core1 to draw the LCD
        gameFrame();					        //Now do our game logic in Core0
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

void gameFrame() { 
    if (paused == true) {
        return;
    }
    
    //Wait for Core1 to begin rendering the frame
    while(frameDrawing == false) {			
        delayMicroseconds(1);				      //Do nothing (arduino doesn't like empty while()s
    }

    // Game Logic that doesn't access video memory
    HandleButtons();
    Animate();
    
    // Wait for Core 1 to finish drawing
    while(frameDrawing == true) {			
        delayMicroseconds(1);
    }

    // Game Logic that requires video memory access. We have about 15ms in which to do this before the next frame starts
    setSpriteWindow(0, 0, 119, 119);    //Set window in which sprites can appear 

    // Draw the ship
    csShip->BitBlit();

    // Draw the bullets
    for (uint8_t n = 0; n < 5; n++)
        if (csBullet[n]->IsAlive())
            csBullet[n]->BitBlit();

    setWindow(screenX, 0);
}


//Every 33ms, set the flag that it's time to draw a new frame (it's possible system might miss one but that's OK)
bool timer_isr(struct repeating_timer *t) {				
    nextFrameFlag = true;
    return true;
}

//////////////////////////////////////////////////////////////////////////
// ResetGame
// Initializes all of the variables used in the game
//////////////////////////////////////////////////////////////////////////
void ResetGame()
{
    // set up the player's ship
    ship.x = 0;
    ship.y = 56;
    csShip->SetX(ship.x);
    csShip->SetY(ship.y);
    csShip->SetXSpeed(1);
    csShip->SetYSpeed(1);

    // set up the bullets
    for (int n = 0; n < 5; n++) 
    {
        csBullet[n]->SetAlive(false);
        csBullet[n]->SetXSpeed(3);
        csBullet[n]->SetX(0);
        csBullet[n]->SetY(0);
    }

    bulletIdx = 0;
}

//////////////////////////////////////////////////////////////////////////
// HandleButtons
// Checks and handles button events
//////////////////////////////////////////////////////////////////////////
void HandleButtons()
{
    if (button(right_but)) {
        ship.x += csShip->GetXSpeed();
        if (ship.x > SCREEN_W) ship.x = SCREEN_W;
        csShip->SetX(ship.x);
    }

    if (button(left_but)) {
        ship.x -= csShip->GetXSpeed();
        if (ship.x < 0) ship.x = 0;
        csShip->SetX(ship.x);
    }

    if (button(up_but)) {
        ship.y -= csShip->GetYSpeed();
        if (ship.y < 0) ship.y = 0;
        csShip->SetY(ship.y);
    }

    if (button(down_but)) {
        ship.y += csShip->GetYSpeed();
        if (ship.y > SCREEN_H) ship.y = SCREEN_H;
        csShip->SetY(ship.y);
    }

    if (button(A_but)) {
        bulletIdx++;
        if (bulletIdx > 4) bulletIdx = 0;

        if (!csBullet[bulletIdx]->IsAlive()) 
        {
            csBullet[bulletIdx]->SetX(csShip->GetX() + BULLET_X_OFFSET);
            csBullet[bulletIdx]->SetY(csShip->GetY() + BULLET_Y_OFFSET);
            csBullet[bulletIdx]->SetAlive(true);
        }    
    }
}

//////////////////////////////////////////////////////////////////////////
// Animate
// Main Animation Routine
//////////////////////////////////////////////////////////////////////////
void Animate() {
    uint8_t n;                                    // A counter that we can reuse

    // Move the player's bullets
    for (n = 0; n < 5; n++) {
        if (csBullet[n]->IsAlive()) 
        {
            csBullet[n]->IncX(csBullet[n]->GetXSpeed());
            if (csBullet[n]->GetX() > 128) 
                csBullet[n]->SetAlive(false);
        }
    }

    // If viewport is at x=0, buffer the next map page
    if (screenX == 0) {
        if (curMapPage != lastMapPage)            // Make sure we're not on the last page in the level map
            LoadTilePage(curMapPage + 1, 1);      // Buffer the next map page into upper right of the name table
        else 
            scrolling_paused = true;              // We're at the end of the map, so stop scrolling (and queue boss fight!)
    }

    // Scroll the screen
    if (!scrolling_paused) {
        screenX++;                                // Advance the viewport
        mapScrollX++;                             // Advance the counter that keeps track of where we are on the level map
        if (screenX == SCREEN_W) {                // X = 120 indicates that we've scrolled past one full screen...
            curMapPage++;                         // Increment tile map screen counter
            LoadTilePage(curMapPage, 0);          // Buffer the map page we're currently on into upper left of the name table
            screenX = 0;                          // Reset viewport x to 0
        } 
    }
}

//////////////////////////////////////////////////////////////////////////
// LoadTilePage
// Loads a screen of tiles onto the name table
//////////////////////////////////////////////////////////////////////////
void LoadTilePage(uint16_t srcPage, uint8_t dstPage)
{
    for (uint8_t x = 0; x < NUM_TILE_COLS; x++) 
        for (uint8_t y = 0; y < NUM_TILE_ROWS; y++) {
            uint8_t tileIdx = tilemap[x+srcPage*NUM_TILE_COLS+y*mapWidth];
            drawTile(x+dstPage*NUM_TILE_COLS, y, tileIdx, paletteMap[tileIdx]);
        }
}
