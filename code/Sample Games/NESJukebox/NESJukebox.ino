#include <gameBadgePico.h>
#include <GBAudio.h>
#include "apu_capture.h"

#define MAX_FILES       8
#define FILENAME_LEN    13

struct repeating_timer timer60Hz;           // Audio track timer
struct repeating_timer timerWFGenerator;    // Timer for the waveform generator - 125KHz

int gameLoopState = 0;
bool displayPauseState = false;
volatile int nextFrameFlag = 0;             //The 30Hz IRS incs this, Core0 Loop responds when it reaches 2 or greater
bool drawFrameFlag = false;                 // When Core0 Loop responses to nextFrameFlag, it sets this flag so Core1 will know to render a frame
bool frameDrawing = false;                  // Set TRUE when Core1 is drawing the display. Core0 should wait for this to clear before accessing video memory


GBAudio audio;

uint8_t cursorX = 0;
uint8_t cursorY = 2;

uint8_t ALLBLACK[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };


void setup() {

    gamebadge3init();


    loadRGB("NEStari.pal");
    loadPalette("basePalette.dat");
    loadPattern("nesjukebox.nes", 0, 512);

    // Waveform generator timer - 64us = 15625Hz
    add_repeating_timer_us(WAVE_TIMER, wavegen_callback, NULL, &timerWFGenerator);

    // NTSC audio timer
    add_repeating_timer_us(AUDIO_TIMER, sixtyHz_callback, NULL, &timer60Hz);

    // Set up debouncing for UDLR
    setButtonDebounce(up_but, true, 1);
    setButtonDebounce(down_but, true, 1);
    setButtonDebounce(left_but, true, 1);
    setButtonDebounce(right_but, true, 1);

    LoadTilePage(0, 0, ALLBLACK);

    audio.AddTrack(smb, 0);
    audio.AddTrack(smb2, 1);
    audio.AddTrack(megaman, 2);
    audio.AddTrack(ddragon, 3);
    audio.AddTrack(gradius, 4);
}

void setup1() {

    
}

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
* THis is the main game loop, which runs at 30Hz.
*************************************************************************/
void GameLoop() {
    // Handle Player Input
    HandleButtons();

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
        audio.PlayTrack(selectedTrack, true);          
    }

    if (button(B_but)) {
        for (int i = 0; i < 8; i++) {
            if (audio.IsTrackPlaying(i)) {
                audio.StopTrack(i);
            }
        }
    }

    if (button(C_but)) {
    }

    if (button(start_but)) {
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