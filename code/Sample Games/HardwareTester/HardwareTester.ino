#include <gameBadgePico.h>
#include <GBAudio.h>
#include "hardware/adc.h"
#include "apu_capture.h"

#define SCREEN_W            120             // Width of the screen, in pixels
#define SCREEN_H            120             // Height of the screen, in pixels

struct repeating_timer timer60Hz;           // Audio track timer
struct repeating_timer timerWFGenerator;    // Timer for the waveform generator - 125KHz

bool paused = false;                        // Player pause, also you can set pause=true to pause the display while loading files
volatile int nextFrameFlag = 0;             //The 30Hz IRS incs this, Core0 Loop responds when it reaches 2 or greater
bool drawFrameFlag = false;                 // When Core0 Loop responses to nextFrameFlag, it sets this flag so Core1 will know to render a frame
bool frameDrawing = false;                  // Set TRUE when Core1 is drawing the display. Core0 should wait for this to clear before accessing video memory
int gameLoopState = 0;


GBAudio audio;

uint8_t ALLBLACK[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };


/*************************************************************************
* setup
* Set up function for Core 0. Use this to initialize the gameBadge3
* library, load graphics, audio, and another needed initializations
*************************************************************************/
void setup() {

    // Initialize the gameBadge3
    gamebadge3binit();

    // Waveform generator timer - 64us = 15625Hz
    add_repeating_timer_us(WAVE_TIMER, wavegen_callback, NULL, &timerWFGenerator);

    // NTSC audio timer
    add_repeating_timer_us(AUDIO_TIMER, sixtyHz_callback, NULL, &timer60Hz);
    
    // Set up debouncing for UDLR
    setButtonDebounce(up_but, false, 1);
    setButtonDebounce(down_but, false, 1);
    setButtonDebounce(left_but, false, 1);
    setButtonDebounce(right_but, false, 1);
    setButtonDebounce(A_but, false, 1);
    setButtonDebounce(B_but, false, 1);
    setButtonDebounce(C_but, false, 1);
    setButtonDebounce(start_but, false, 1);
    setButtonDebounce(select_but, false, 1);

    // Import audio
    audio.AddTrack(ddragon, 0);
}

/*************************************************************************
* setup1
* Setup function for Core1. Nothing to do in here
*************************************************************************/
void setup1() {
    
}

/*************************************************************************
* loop
* Main logic loop for Core 0. This tracks the 60Hz timer, and sets a flag
* every other frame (30Hz) to tell Core 1 to start drawing. It will also
* fire off the game loop at 30Hz.
*************************************************************************/
void loop() {
    if (nextFrameFlag > 1) {			//Counter flag from the 60Hz ISR, 2 ticks = 30 FPS
    
		nextFrameFlag -= 2;				//Don't clear it, just sub what we're "using" in case we missed a frame (maybe this isn't best with slowdown?)
		LCDsetDrawFlag();				//Tell core1 to start drawing a frame

		if (gameLoopState == 0) {		//If the last game logic loop has finished, start the next
			gameLoopState = 1;			
		}

		serviceDebounce();				//Debounce buttons
	}
	gameLoopLogic();					//Check this every loop frame
}

/*************************************************************************
* loop1
* Main loop for Core 1. Draws the LCD.
*************************************************************************/
void loop1() {
    LCDlogic();
}

/*************************************************************************
* gameLoopLogic
* Advances the state machine for the main game logic.
*************************************************************************/
void gameLoopLogic() {
	switch(gameLoopState) {
	
		case 0:
			//Do nothing (is set to 1 externally)
		break;
		
		case 1:		
			serviceAudio();						//Service PCM audio file streaming while Core1 draws the screens (gives Core0 something to do while we wait for vid mem access)
			gameLoopState = 2;
		break;		
		
		case 2:									//Done with our audio, now waiting for frame to finish rendering (we can start our logic during the final DMA)
			if (getRenderStatus() == false) {	//Wait for core 1 to finish drawing frame
				gameLoopState = 3;				//Jump to next check
			}
		break;
		
		case 3:									//Frame started?
			GameLoop();	
			gameLoopState = 0;					//Done, wait for next frame flag
		break;			
	}	
}

/*************************************************************************
* GameLoop
* This is the main game loop, for when the player is actively playing the
* game. 
*************************************************************************/
void GameLoop() { 
    LoadTilePage(0, 0, ALLBLACK);

    //        0123456789ABCDE
    drawText("   HW TESTER", 0, 0, false);

    if (!audio.IsTrackPlaying(0)) {
        //        0123456789ABCDE
        drawText(" SELECT AND A  ", 0, 12, false);
        drawText(" TO TEST AUDIO ", 0, 13, false);
    } else {
        //        0123456789ABCDE
        drawText(" AUDIO PLAYING ", 0, 11, false);
        drawText(" SELECT AND B  ", 0, 12, false);
        drawText(" TO STOP AUDIO ", 0, 13, false);
    }
    HandleButtons();
}

/*************************************************************************
* HandleButtons
* Checks and handles button events
*************************************************************************/
void HandleButtons()
{
    if (button(right_but)) {                        // Player pressing right button?
    //        0123456789ABCDE
    drawText("     RIGHT", 0, 7, false);
    }

    if (button(left_but)) {                         // Player pressing left button?
    //        0123456789ABCDE
    drawText("     LEFT", 0, 7, false);
    }

    if (button(up_but)) {                           // Player pressing up button?
    //        0123456789ABCDE
    drawText("       UP", 0, 7, false);
    }

    if (button(down_but)) {                         // Player pressing down button?
    //        0123456789ABCDE
    drawText("     DOWN", 0, 7, false);
    }

    if (button(B_but)) {                            // If the player presses B
    //        0123456789ABCDE
    drawText("       B", 0, 7, false);
    }

    if (button(A_but)) {                            // If the player presses A
    //        0123456789ABCDE
    drawText("       A", 0, 7, false);
    
    }

    if (button(C_but)) {                            // If the player presses C
    //        0123456789ABCDE
    drawText("       C", 0, 7, false);
    }

    if (button(start_but)) {                        // If the player presses START
    //        0123456789ABCDE
    drawText("     START", 0, 7, false);
    }

    if (button(select_but)) {                       // If the player presses SELECT
    //        0123456789ABCDE
    drawText("    SELECT", 0, 7, false);
    }

    if (button(select_but) && button(A_but)) {                       // If the player presses SELECT
        audio.PlayTrack(0);    
    }

    if (button(select_but) && button(B_but)) {                       // If the player presses SELECT
        audio.StopTrack(0);    
    }
}


/*************************************************************************
* LoadTilePage
* Loads a screen of tiles onto the name table
*************************************************************************/
void LoadTilePage(uint16_t srcPage, uint8_t dstPage, uint8_t *tiles)
{
    for (uint8_t x = 0; x < 15; x++) 
        for (uint8_t y = 0; y < 15; y++) {
            uint8_t tileIdx = tiles[x+srcPage*15+y*15];
            drawTile(x+dstPage*15, y, tileIdx, 0);
        }
}

bool sixtyHz_callback(struct repeating_timer *t) {
    audio.ServiceTracks();	
	nextFrameFlag += 1;								//When main loop sees this reach 2, run 30Hz game logic	
    return true;
}

bool wavegen_callback(struct repeating_timer *t) {
    audio.ProcessWaveforms();
    return true;
}