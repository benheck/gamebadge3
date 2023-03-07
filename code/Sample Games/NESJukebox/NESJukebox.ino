#include <gameBadgePico.h>
#include "FamiPlayer.h"
#include "doubledragon.h"
#include "megaman.h"
#include "gradius.h"
#include "smb1.h"
#include "smb2.h"

struct repeating_timer timer30Hz;           // This runs the game clock at 30Hz
struct repeating_timer timer60Hz;           // Audio track timer
struct repeating_timer timerWFGenerator;    // Timer for the waveform generator - 125KHz

bool paused = false;                        // Player pause, also you can set pause=true to pause the display while loading files
bool nextFrameFlag = false;                 // The 30Hz ISR sets this flag, Core0 Loop responds to it
bool drawFrameFlag = false;                 // When Core0 Loop responses to nextFrameFlag, it sets this flag so Core1 will know to render a frame
bool frameDrawing = false;                  // Set TRUE when Core1 is drawing the display. Core0 should wait for this to clear before accessing video memory

FamiPlayer famiPlayer;
uint16_t trackDoubleDragon;
uint16_t trackMegaMan;
uint16_t trackGradius;
uint16_t trackSMB1;
uint16_t trackSMB2;

uint8_t cursorX = 0;
uint8_t cursorY = 2;

void setup() {
    paused = true;                          // Prevent 2nd core from drawing the screen until file system is booted

    gamebadge3init();
    
    if (loadRGB("NEStari.pal")) {
        loadPalette("basePalette.dat");
        loadPattern("nesjukebox.nes", 0, 512);
        
        paused = false;
    }

    // Set up debouncing for UDLR
    setButtonDebounce(up_but, true, 1);
    setButtonDebounce(down_but, true, 1);
    setButtonDebounce(left_but, true, 1);
    setButtonDebounce(right_but, true, 1);

    // Load audio tracks
    trackDoubleDragon = famiPlayer.AddTrack(doubledragon1, doubledragon2, doubledragon3, doubledragon4, sizeof(doubledragon1)/2);
    trackMegaMan = famiPlayer.AddTrack(megaman1, megaman2, megaman3, megaman4, sizeof(megaman1)/2);
    trackGradius = famiPlayer.AddTrack(gradius1, gradius2, gradius3, gradius4, sizeof(gradius1)/2);
    trackSMB1 = famiPlayer.AddTrack(smb11, smb12, smb13, smb14, sizeof(smb11)/2);
    trackSMB2 = famiPlayer.AddTrack(smb21, smb22, smb23, smb24, sizeof(smb21)/2);    
}

void setup1() {
    clearSprite();

    // Start the timer for updating the display
    add_repeating_timer_ms(33, timer_isr, NULL, &timer30Hz);

    // Waveform generator timer - 8us = 125000Hz
    add_repeating_timer_us(8, wavegen_callback, NULL, &timerWFGenerator);

    // NTSC audio timer
    add_repeating_timer_us(16666, ntsc_audio_callback, NULL, &timer60Hz);

}

void loop() {
    if (nextFrameFlag == true) {		    //Flag from the 30Hz ISR?
        nextFrameFlag = false;			    //Clear that flag		
        drawFrameFlag = true;			    //and set the flag that tells Core1 to draw the LCD

        GameLoop();					        //Now do our game logic in Core0
            
        serviceDebounce();                  // Debounce buttons - must be called every frame
    }
}

void loop1() {
    // If flag set to draw display
    if (drawFrameFlag == true) {			
        drawFrameFlag = false;				//Clear flag
        
        if (paused == true) {               //If system paused, return (no draw)
            return;
        }

        frameDrawing = true;                //Set flag that we're drawing a frame. Core0 should avoid accessing video memory if this flag is TRUE
        sendFrame();
        frameDrawing = false;               //Clear flag. Core0 is now free to access screen memory (I guess this core could as well?)
    }
}

/*************************************************************************
* GameLoop
* THis is the main game loop, which runs at 30Hz.
*************************************************************************/
void GameLoop() { 
    // 1. Erase Objects (i.e. text)
    //------------------------------

    // 2. Get Input
    //--------------
    HandleButtons();

    // 3. Move Objects
    //-----------------


    // 4. Do Collisions
    //------------------
   
    
    // 5. Scan background
    //--------------------


    // 6. Draw Objects
    //-----------------
    drawText("NES Jukebox", 2, 0);
    drawText("Double Dragon", 1, 2);
    drawText("Mega Man", 1, 3);
    drawText("Gradius", 1, 4);
    drawText("Super Mario 1", 1, 5);
    drawText("Super Mario 2", 1, 6);

    drawText("A = Play | Stop", 0, 14);
    drawSprite(cursorX << 3, cursorY << 3, 1, 0);
}

/*************************************************************************
* HandleButtons
* Checks and handles button events
*************************************************************************/
void HandleButtons()
{
    if (button(right_but)) {
        
    }

    if (button(left_but)) {
        
    }

    if (button(up_but)) {
        if (cursorY > 2) cursorY--;
    }

    if (button(down_but)) {
        if (cursorY < 6) cursorY++;
    }

    if (button(A_but)) {
        int selectedTrack = cursorY - 2;
        
        if (famiPlayer.tracks[selectedTrack].playing)
            famiPlayer.StopTrack(selectedTrack);
        else
        {
            for (int i=0; i < 4; i++) famiPlayer.StopTrack(i);
            famiPlayer.PlayTrack(selectedTrack);
        }
            
    }

    if (button(B_but)) {

    }

    if (button(C_but)) {

    }
}

//Every 33ms, set the flag that it's time to draw a new frame (it's possible system might miss one but that's OK)
bool timer_isr(struct repeating_timer *t) {				
    nextFrameFlag = true;
    return true;
}

bool wavegen_callback(struct repeating_timer *t) {
    famiPlayer.ProcessWave();
    return true;
}

bool ntsc_audio_callback(struct repeating_timer *t) {
    famiPlayer.serviceTracks();
    return true;
}