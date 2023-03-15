#include <gameBadgePico.h>
#include "FamiPlayer.h"
#include "SdFat.h"

#define MAX_FILES       8
#define FILENAME_LEN    13

struct repeating_timer timer30Hz;           // This runs the game clock at 30Hz
struct repeating_timer timer60Hz;           // Audio track timer
struct repeating_timer timerWFGenerator;    // Timer for the waveform generator - 125KHz

bool displayPause = false;
bool paused = false;                        // Player pause, also you can set pause=true to pause the display while loading files
bool nextFrameFlag = false;                 // The 30Hz ISR sets this flag, Core0 Loop responds to it
bool drawFrameFlag = false;                 // When Core0 Loop responses to nextFrameFlag, it sets this flag so Core1 will know to render a frame
bool frameDrawing = false;                  // Set TRUE when Core1 is drawing the display. Core0 should wait for this to clear before accessing video memory

FamiPlayer famiPlayer;

uint8_t cursorX = 0;
uint8_t cursorY = 2;
FatFile root;
FatFile audiofile;
String fileList[MAX_FILES];

void setup() {
    Serial.begin(9600);

    gamebadge3init();

    // Start the timer for updating the display
    add_repeating_timer_ms(-33, timer_isr, NULL, &timer30Hz);

    // Waveform generator timer - 8us = 125000Hz
    add_repeating_timer_us(8, wavegen_callback, NULL, &timerWFGenerator);

    // NTSC audio timer
    add_repeating_timer_us(-16666, ntsc_audio_callback, NULL, &timer60Hz);

    
    if (loadRGB("NEStari.pal")) {
        loadPalette("basePalette.dat");
        loadPattern("nesjukebox.nes", 0, 512);
    }

    // Set up debouncing for UDLR
    setButtonDebounce(up_but, true, 1);
    setButtonDebounce(down_but, true, 1);
    setButtonDebounce(left_but, true, 1);
    setButtonDebounce(right_but, true, 1);

    // Load audio tracks
    famiPlayer.AddTrack("/jukebox/smb1.dat", 0);
    famiPlayer.AddTrack("/jukebox/smb2.dat", 1);
    famiPlayer.AddTrack("/jukebox/megaman.dat", 2);
    famiPlayer.AddTrack("/jukebox/doubledragon.dat", 3);
    famiPlayer.AddTrack("/jukebox/gradius.dat", 4);
}

void setup1() {

    
}

void loop() {
    if (nextFrameFlag == true) {		    //Flag from the 30Hz ISR?
        nextFrameFlag = false;			    //Clear that flag		
        drawFrameFlag = true;			      //and set the flag that tells Core1 to draw the LCD

        GameLoop();					            //Now do our game logic in Core0
            
        serviceDebounce();              // Debounce buttons - must be called every frame
    }
}

void loop1() {
    // If flag set to draw display
    if (drawFrameFlag == true) {			
        drawFrameFlag = false;				  //Clear flag
        
        if (displayPause == true) {     //If system paused, return (no draw)
            return;
        }

        frameDrawing = true;            //Set flag that we're drawing a frame. Core0 should avoid accessing video memory if this flag is TRUE
        sendFrame();
        frameDrawing = false;           //Clear flag. Core0 is now free to access screen memory (I guess this core could as well?)
    }
}

/*************************************************************************
* GameLoop
* THis is the main game loop, which runs at 30Hz.
*************************************************************************/
void GameLoop() { 
    if (!displayPause) {                  // If the display is being actively refreshed we need to wait before accessing video RAM
        while(!frameDrawing) {            // Wait for Core1 to begin rendering the frame
            delayMicroseconds(1);         // Do almost nothing (arduino doesn't like empty while()s)
        }

        // Do game logic that doesn't access video memory
        HandleButtons();

        while(frameDrawing) {             // Done with non-video logic; wait for Core 1 to finish drawing
            delayMicroseconds(1);
        }
    }
    
    // Now access video memory. We have ~15ms to do this before the next frame starts
    //.       0123456789ABCDE
    drawText(  "NES Jukebox",   2, 0);
    drawText( "Super Mario",    1, 2);
    drawText( "Super Mario 2",  1, 3);
    drawText( "Mega Man",       1, 4);
    drawText( "Double Dragon",  1, 5);
    drawText( "Gradius",        1, 6);
    drawText("A=Play | B=Stop", 0, 14);

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
        famiPlayer.PlayTrack(selectedTrack);          
    }

    if (button(B_but)) {
        for (int i = 0; i < 8; i++) {
            if (famiPlayer.IsPlaying(i)) {
                famiPlayer.StopTrack(i);
            }
        }
    }

    if (button(C_but)) {
    }

    if (button(start_but)) {
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