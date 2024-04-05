#include <EEPROM.h>
#include <gameBadgePico.h>
#include <GBAudio.h>
#include "music.h"
#include "hardware/adc.h"

#define SCREEN_W            120             // Width of the screen, in pixels
#define SCREEN_H            120             // Height of the screen, in pixels
#define NUM_TILE_COLS       15              // # of tiles in each row
#define NUM_TILE_ROWS       15              // # of rows of tiles
#define BLK_SIZE            5               // Size of each tetris block, in pixels
#define SM_BLK_SIZE         4               // Size of the small tetris blocks, in pixels
#define NUM_BLOCKS_X        10              // # of blocks in each row
#define NUM_BLOCKS_Y        20              // # of rows in the playing field
#define BOARD_X             35              // Upper-left x pixel of the play field
#define BOARD_Y             18              // Upper-left y pixel of the play field
#define START_X_BL          5               // The starting grid X position for a new tetromino
#define START_Y_BL          0               // The starting grid Y position for a new tetromino
#define SOFTDROP_SPD        1               // How quickly the blocks fall when soft drop is on (in FPS)
#define INIT_SLIDE_SPD      8               // How much time to wait before starting a slide (in frames)
#define SLIDE_SPD           1               // How fast should the slide occur (in frames)
#define LINES_X             BOARD_X + NUM_BLOCKS_X * BLK_SIZE / 2
#define LINES_Y             BOARD_Y - 9
#define LEVEL_X             90
#define LEVEL_Y             89
#define NXT_PIECE_X         103
#define NXT_PIECE_Y         68
#define SCORE_X             87              // Upper-left x pixel of the current score box
#define SCORE_Y             32              // Upper-left y pixel of the current score box
#define HISCORE_X           87              // Upper-left x pixel of the high score box
#define HISCORE_Y           17              // Upper-left y pixel of the high score box
#define STATS_X             13              // Upper-left x pixel of the Stats box
#define STATS_Y             35              // Upper-left y pixel of the Stats box
#define LOCK_BYPASS_FR      3
#define END_ANIM_TICKS      2               // # frames between game over animation sequences
#define CLR_ANIM_TICKS      1               // # frames between clear line animation sequence
#define CUR_BLINK_RATE      15              // # frames per second that the cursor blinks in the menu pages
#define CHAR_TICK_RATE      4               // # frames that it takes for a character to tick over when holding down the button

#define PIECE_I             0
#define PIECE_J             1
#define PIECE_L             2
#define PIECE_O             3
#define PIECE_S             4
#define PIECE_T             5
#define PIECE_Z             6

#define ROT_0               0
#define ROT_90              1
#define ROT_180             2
#define ROT_270             3

#define SND_MUSIC1          0
#define SND_MUSIC2          1
#define SND_MUSIC3          2
#define SND_MOVE            3
#define SND_ROTATE          4
#define SND_CLEAR           5
#define SND_CLEAR4          6
#define SND_DROP            7
#define SND_PAUSE           8
#define SND_GAMEOVER        9
#define SND_LEVELUP         10
#define SND_HISCORE         11

struct repeating_timer timer60Hz;           // Audio track timer
struct repeating_timer timerWFGenerator;    // Timer for the waveform generator - 125KHz
struct Point {
    int8_t x;
    int8_t y;
};

struct ScoreEntry {
    String name;
    uint32_t score;
    uint8_t level;
};

enum gamestate { splashScreen, titleScreen, gameScreen, pauseScreen, optionsScreen, congratsScreen, levelScreen };
enum IngameStates { playing, dead, clear };

bool paused = false;                        // Player pause, also you can set pause=true to pause the display while loading files
volatile int nextFrameFlag = 0;             //The 30Hz IRS incs this, Core0 Loop responds when it reaches 2 or greater
bool drawFrameFlag = false;                 // When Core0 Loop responses to nextFrameFlag, it sets this flag so Core1 will know to render a frame
bool frameDrawing = false;                  // Set TRUE when Core1 is drawing the display. Core0 should wait for this to clear before accessing video memory

uint8_t piece;
uint8_t next;
uint8_t rotation;
uint8_t level = 0;
uint32_t score;
uint32_t hiscore;
uint16_t lines;
uint8_t softDrop;
uint8_t softDropFrames;
bool softDropReset;
uint8_t speed;                              // Speed that pieces fall - starts at 24 frames per second
uint8_t lockDelay;
bool prevLockDelay;
bool rInSlide = false;                      // Triggers the start of a right slide
bool lInSlide = false;                      // Triggers the start of a left slide
uint8_t rSlideTimer = 0;                    // Right slide frequency timer
uint8_t rSlideInitTimer = 0;                // Timer to delay before sliding right
uint8_t lSlideTimer = 0;                    // Left slide frequency timer 
uint8_t lSlideInitTimer = 0;                // Timer to delay before sliding left
uint8_t frameCounter = 0;                   // Keeps track of how many frames we've been through so we know when to move the piece down
uint8_t lockBypass;
uint8_t blockPalette = 1;
int8_t x, y;
uint8_t board[NUM_BLOCKS_X][NUM_BLOCKS_Y];
uint16_t stats[7] = { 0 };
int8_t endAnimTimer = 0;
uint8_t endAnimRowCount = 0;
int8_t clearAnimTimer = 0;
uint8_t clearAnimBlock = 0;
int8_t clearList[4] = { -1, -1, -1, -1 };
bool doPause = false;
bool sfxOn = true;
bool musicOn = true;
bool hideNext = false;
GBAudio audio;
char name[7] = "------";
uint8_t currentChar = 0;
uint8_t selectedMusicOption = SND_MUSIC1;
int gameLoopState = 0;
bool displayPauseState = false;
ScoreEntry hiScoreTable[3];

gamestate currentState = splashScreen;
gamestate lastState = splashScreen;
IngameStates igs = playing;

uint8_t ALLBLACK[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
uint8_t BGTILES[] = { 16,17,18,19,196,216,216,232,217,197,198,19,16,27,22,18,16,22,24,4,0,0,0,0,0,28,217,217,197,198,23,19,16,27,1,2,2,2,2,2,3,0,0,0,215,18,18,18,25,4,0,0,0,0,0,5,0,0,0,231,2,2,2,2,12,0,0,0,0,0,5,0,0,0,231,0,0,0,0,13,0,0,0,0,0,5,0,0,0,214,0,0,0,0,13,0,0,0,0,0,15,229,229,229,230,0,0,0,0,13,0,0,0,0,0,5,0,0,0,215,0,0,0,0,13,0,0,0,0,0,5,0,0,0,231,0,0,0,0,13,0,0,0,0,0,5,0,0,0,214,0,0,0,0,13,0,0,0,0,0,15,229,229,229,230,0,0,0,0,13,0,0,0,0,0,5,0,0,0,215,0,0,0,0,13,0,0,0,0,0,5,0,0,0,214,0,0,0,0,13,0,0,0,0,0,29,225,225,241,226,7,7,7,7,14,7,7,7,7,7,8,19,19,25,30 };
uint8_t OPTIONS[] = { 20,21,20,27,26,20,30,20,222,22,19,25,30,25,27,24,192,193,193,193,193,193,193,194,23,18,16,27,22,23,16,208,209,209,209,209,209,209,210,219,23,18,20,21,19,18,16,22,18,24,17,19,223,22,18,19,23,24,17,19,25,30,20,21,25,26,18,192,193,193,193,193,193,194,18,23,23,24,17,23,19,16,233,32,32,32,32,32,234,23,19,223,26,16,30,18,18,233,32,32,32,32,32,234,30,223,22,18,18,25,26,25,233,32,32,32,32,32,234,21,23,16,22,20,21,223,22,233,0,0,0,0,0,234,17,19,19,23,24,17,25,27,208,209,209,209,209,209,210,22,19,18,221,22,23,23,20,27,22,16,30,23,25,30,23,18,192,193,194,219,192,193,193,194,18,192,193,193,194,19,23,208,209,210,18,208,209,209,210,22,208,209,209,210,19,221,22,23,25,26,18,20,21,19,25,220,22,25,26,18,18,25,220,22,223,22,24,17,18,25,27,27,22,223,22 };
uint8_t CONGRAT[] = { 20,21,25,27,26,16,30,25,222,22,19,25,30,25,27,24,243,244,244,244,244,244,244,244,244,244,244,244,245,23,16,4,0,0,0,0,0,0,0,0,0,0,0,5,19,18,4,0,0,0,0,0,0,0,0,0,0,0,5,19,25,4,0,0,0,0,0,0,0,0,0,0,0,5,18,23,4,0,0,0,0,0,0,0,0,0,0,0,5,23,19,4,0,0,0,0,0,0,0,0,0,0,0,5,30,223,116,117,117,117,117,117,117,117,117,117,117,117,118,21,23,119,122,122,122,122,122,122,122,122,122,122,122,120,17,19,124,0,0,0,0,0,0,0,0,0,0,32,125,22,19,124,0,0,0,0,0,0,0,0,0,0,32,125,23,18,124,32,32,32,32,32,32,32,32,32,32,32,125,19,23,121,122,122,122,122,122,122,122,122,122,122,122,123,19,221,246,247,247,247,247,247,247,247,247,247,247,247,248,18,18,25,220,22,223,22,24,17,18,25,27,27,22,223,22 };
uint8_t TITLE[] = { 20,21,25,27,26,16,30,25,222,22,19,25,30,25,27,24,17,0,0,18,18,0,0,18,0,18,16,27,22,23,16,27,22,0,0,0,0,0,0,0,0,18,0,0,19,18,0,0,0,0,0,0,0,0,0,0,0,0,0,19,22,0,0,0,0,0,0,0,0,0,0,0,0,0,18,23,0,0,0,0,0,0,0,0,0,0,0,0,0,23,19,0,0,0,0,0,0,0,0,0,0,0,25,27,30,223,22,0,0,0,0,0,0,0,0,0,0,0,20,21,23,0,0,0,0,0,0,0,0,0,0,0,0,24,17,19,0,0,0,0,0,0,0,0,0,0,0,0,16,22,19,0,0,0,0,0,0,0,0,0,0,0,0,19,23,18,23,0,0,0,0,0,0,0,0,0,0,0,18,19,23,223,27,22,0,0,0,0,16,22,0,0,0,0,19,221,22,23,25,26,0,20,21,19,0,0,0,25,26,18,18,25,220,22,223,22,24,17,18,25,27,27,22,223,22 };
uint8_t LEVEL[] = { 20,21,25,27,26,16,30,25,222,22,19,25,30,25,27,24,243,244,244,244,244,244,244,244,244,244,244,244,245,23,16,4,0,0,0,0,0,0,0,0,0,0,0,5,19,18,4,0,0,0,0,0,0,0,0,0,0,0,5,19,25,4,0,0,0,112,112,112,112,112,113,0,0,5,18,23,4,0,0,0,112,112,112,112,112,113,0,0,5,23,19,4,0,0,0,114,114,114,114,114,126,0,0,5,30,223,116,117,117,117,117,117,117,117,117,117,117,117,118,21,23,119,122,122,122,122,122,122,122,122,122,122,122,120,17,19,124,0,0,0,0,0,0,0,0,0,0,0,125,22,19,124,0,0,0,0,0,0,0,0,0,0,0,125,23,18,124,0,0,0,0,0,0,0,0,0,0,0,125,19,23,121,122,122,122,122,122,122,122,122,122,122,122,123,19,221,246,247,247,247,247,247,247,247,247,247,247,247,248,18,18,25,220,22,223,22,24,17,18,25,27,27,22,223,22 };

uint8_t alphaSequence[] = { 0x2D, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2C, 0x2F, 0x28, 0x29, 0x22, 0x2E, 0x20 };

const Point piece_i[4][4] = {   { {-2, 0}, {-1, 0}, {0, 0}, {1, 0} },
                                { {0, -2}, {0, -1}, {0, 0}, {0, 1} },
                                { {-2, 0}, {-1, 0}, {0, 0}, {1, 0} },
                                { {0, -2}, {0, -1}, {0, 0}, {0, 1} } };

const Point piece_j[4][4] = {   { {-1, 0}, {0, 0}, {1, 0}, {1, 1} },
                                { {0, -1}, {0, 0}, {0, 1}, {-1, 1} },
                                { {-1, 0}, {0, 0}, {1, 0}, {-1, -1} },
                                { {0, -1}, {0, 0}, {0, 1}, {1, -1} } };

const Point piece_l[4][4] = {   { {-1, 0}, {0, 0}, {1, 0}, {-1, 1} },
                                { {0, -1}, {0, 0}, {0, 1}, {-1, -1} },
                                { {-1, 0}, {0, 0}, {1, 0}, {1, -1} },
                                { {0, -1}, {0, 0}, {0, 1}, {1, 1} } };

const Point piece_o[4][4] = {   { {-1, 1}, {0, 1}, {-1, 0}, {0, 0} },
                                { {-1, 1}, {0, 1}, {-1, 0}, {0, 0} },
                                { {-1, 1}, {0, 1}, {-1, 0}, {0, 0} },
                                { {-1, 1}, {0, 1}, {-1, 0}, {0, 0} } };

const Point piece_s[4][4] = {   { {-1, 1}, {0, 1}, {1, 0}, {0, 0} },
                                { {0, -1}, {1, 0}, {1, 1}, {0, 0} },
                                { {-1, 1}, {0, 1}, {1, 0}, {0, 0} },
                                { {0, -1}, {1, 0}, {1, 1}, {0, 0} } };

const Point piece_t[4][4] = {   { {-1, 0}, {0, 0}, {1, 0}, {0, 1} },
                                { {0, -1}, {0, 0}, {0, 1}, {-1, 0} },
                                { {-1, 0}, {0, 0}, {1, 0}, {0, -1} },
                                { {0, -1}, {0, 0}, {0, 1}, {1, 0} } };

const Point piece_z[4][4] = {   { {-1, 0}, {0, 0}, {0, 1}, {1, 1} },
                                { {0, 0}, {0, 1}, {1, 0}, {1, -1} },
                                { {-1, 0}, {0, 0}, {0, 1}, {1, 1} },
                                { {0, 0}, {0, 1}, {1, 0}, {1, -1} } };


/*************************************************************************
* setup
* Set up function for Core 0. Use this to initialize the gameBadge3
* library, load graphics, audio, and another needed initializations
*************************************************************************/
void setup() {

    // Initialize the gameBadge3
    gamebadge3init();

    loadRGB("badgeris/badgeris.pal");               // Load the color palette
    loadPalette("badgeris/splash.dat");             // Load the palette table
    loadPattern("badgeris/badgeris.nes", 0, 512);   // Load the sprites and tiles

    // Waveform generator timer - 64us = 15625Hz
    add_repeating_timer_us(WAVE_TIMER, wavegen_callback, NULL, &timerWFGenerator);

    // NTSC audio timer
    add_repeating_timer_us(AUDIO_TIMER, sixtyHz_callback, NULL, &timer60Hz);
    
    // Set up debouncing for UDLR
    setButtonDebounce(up_but, true, 1);
    setButtonDebounce(down_but, false, 1);
    setButtonDebounce(left_but, false, 1);
    setButtonDebounce(right_but, false, 1);

    // Display the Splash Page first
    ChangeState(currentState);

    // Initialize the ADC for the analog seed generator
    adc_init();
    adc_gpio_init(28);
    adc_select_input(2);
    randomSeed(adc_read());

    // Import audio
    audio.AddTrack(music_a, SND_MUSIC1);
    audio.AddTrack(music_b, SND_MUSIC2);
    audio.AddTrack(music_c, SND_MUSIC3);
    audio.AddTrack(snd_move, SND_MOVE);
    audio.AddTrack(snd_rotate, SND_ROTATE);
    audio.AddTrack(snd_drop, SND_DROP);
    audio.AddTrack(snd_clear, SND_CLEAR);
    audio.AddTrack(snd_clear4, SND_CLEAR4);
    audio.AddTrack(snd_pause, SND_PAUSE);
    audio.AddTrack(snd_gameover, SND_GAMEOVER);
    audio.AddTrack(snd_levelup, SND_LEVELUP);
    audio.AddTrack(music_hiscore, SND_HISCORE);

    // Set default high score table
    hiScoreTable[0] = ScoreEntry{ "BEN H.", 10000, 9 };
    hiScoreTable[1] = ScoreEntry{ "KEN", 7500, 5 };
    hiScoreTable[2] = ScoreEntry{ "STUART", 5000, 3 };

    InitHighScoreTable();               // Initialize the high score table, if necessary
    ReadHighScoreTable();               // Load saved scores
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
* gameFrame
* Looks at the current game state machine, and fires off the proper loop
* that handles the game during that state.
*************************************************************************/
void gameFrame() {
    switch (currentState) {
            case splashScreen:
                SplashLoop();
                break;
            case titleScreen:
                TitleLoop();
                break;
            case levelScreen:
                LevelLoop();
                break;
            case gameScreen:
                GameLoop();
                break;
            case pauseScreen:
                PauseLoop();
                break;
            case optionsScreen:
                OptionsLoop();
                break;
            case congratsScreen:
                CongratsLoop();
                break;
            default:
                break;
        }
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
			gameFrame();	
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
    // There are 2 in-game states (igs). One that's set when the game is currently
    // being played (playing). The other state indicates when the player has died
    // in order to kick off the death sequence.
     if (igs == playing) {
        HandleButtons();                                // Handle player input

        // Move the current piece down at the appropriate drop speed
        if (frameCounter >= speed - 1 || (softDrop > 0 && frameCounter >= softDrop))
        {
            if (CanMoveDown()) {
                y++;
                if (softDrop > 0) softDropFrames++;
            }
            frameCounter = 0;
        }
        frameCounter++;
    
        // If we've hit the bottom, and lock delay is not set, and if we're not just 
        // coming out of it, then start the lock delay timer
        if (!CanMoveDown() && lockDelay == 0 && !prevLockDelay )
        {
            lockDelay = speed;                          // Start the lock delay
            prevLockDelay = true;                       // Set lock delay history
        }
        // Else if we hit the bottom and we're just getting out of lock delay OR if
        // we hit the bottom, and the player is holding DOWN after the lock delay bypass
        // counter was reached, freeze the block
        else if (!CanMoveDown() && ((lockDelay == 0 && prevLockDelay) || (button(down_but) && lockBypass > LOCK_BYPASS_FR)))
        {
            FreezeBlock();                              // Lock the piece in place
            if (sfxOn) audio.PlayTrack(SND_DROP);       // Play the drop sound
            //ClearLines();                               // Clear the completed lines
            BuildClearList();
            rotation = ROT_0;                           // Reset the piece rotation
            piece = next;                               // Make the next piece active
            stats[piece]++;                             // Update piece stats
            next = GenerateRandomPiece();               // Generate the next piece
            x = START_X_BL;                             // Reset x to the starting x grid cell
            y = START_Y_BL;                             // Reset y to the starting y grid cell
            softDrop = 0;                               // Reset soft drop speed
            softDropReset = true;                       // Set the soft drop debounce trigger
            prevLockDelay = false;                      // Reset the lock delay previous state
            frameCounter = 0;                           // Reset the frame counter
            score += softDropFrames;                    // Increase score
            softDropFrames = 0;                         // Reset soft drop frame counter
            lockBypass = 0;                             // Reset the lock delay bypass counter
            lockDelay = 0;                              // Reset lock delay
        }
        // Else if we hit the bottom, and the player is holding DOWN, so increment the lock
        // delay bypass counter
        else if (!CanMoveDown() && button(down_but))
        {
            lockBypass++;
        }

        // If we hit the bottom and lock delay is set, tick it down
        if (!CanMoveDown() && lockDelay > 0)
        {
            lockDelay--;                  
        }
    } 
    // This is the second in-game state. If the player has died (the board has filled up),
    // this state is set after the death sequence is played. From here, the we just wait for
    // the player to press Start to take them to the next screen.
    else if (igs == dead) {                             // Player has died
        if (button(start_but))                          // Wait for start button
        {
            if (CheckHighScore(score) > -1)             // If the player has the current high score
                ChangeState(congratsScreen);            // Take them to the congrats screen
            else                                        // Otherwise...
                ChangeState(optionsScreen);             // Go to the options screen to start a new game
        }
        
    }

    // Handle Pause. We must do this outside of the other button handler because it accesses video memory.
    if (doPause) {
        ChangeState(pauseScreen);
        doPause = false;
    }

    // Draw the next piece
    DrawTextSmall("NEXT", NXT_PIECE_X - 10, NXT_PIECE_Y - 10, 0, false);
    if (!hideNext) DrawSmallPiece(NXT_PIECE_X, NXT_PIECE_Y, next);

    // Draw the level counter
    DrawTextSmall("LEVEL", LEVEL_X, LEVEL_Y, 0, false);
    DrawNumSmallLZ(level, 2, LEVEL_X + 10, LEVEL_Y + 7, 0, false);          // Display level count

    // Draw the line counter
    String sLines = "LINES-" + intToStringWithLeadingZeros(lines, 3);
    DrawTextSmall(sLines.c_str(), LINES_X, LINES_Y, 0, true);            // Display the line counter, centered across the play field

    // Draw the Statistics
    DrawTextSmall("STATS", STATS_X, STATS_Y, 0, true);       
    DrawSmallPiece(8, STATS_Y + 8, PIECE_T);
    DrawNumSmallLZ(stats[PIECE_T], 3, 16, STATS_Y + 8, 0, false);
    DrawSmallPiece(8, STATS_Y + 19, PIECE_J);
    DrawNumSmallLZ(stats[PIECE_J], 3, 16, STATS_Y + 19, 0, false);
    DrawSmallPiece(8, STATS_Y + 30, PIECE_Z);
    DrawNumSmallLZ(stats[PIECE_Z], 3, 16, STATS_Y + 30, 0, false);
    DrawSmallPiece(8, STATS_Y + 41, PIECE_O);
    DrawNumSmallLZ(stats[PIECE_O], 3, 16, STATS_Y + 41, 0, false);
    DrawSmallPiece(8, STATS_Y + 52, PIECE_S);
    DrawNumSmallLZ(stats[PIECE_S], 3, 16, STATS_Y + 52, 0, false);
    DrawSmallPiece(8, STATS_Y + 63, PIECE_L);
    DrawNumSmallLZ(stats[PIECE_L], 3, 16, STATS_Y + 63, 0, false);
    DrawSmallPiece(8, STATS_Y + 74, PIECE_I);
    DrawNumSmallLZ(stats[PIECE_I], 3, 16, STATS_Y + 74, 0, false);

    // Draw the Top Score
    DrawTextSmall("TOP", HISCORE_X, HISCORE_Y, 0, false);
    DrawNumSmallLZ(hiscore, 6, HISCORE_X, HISCORE_Y + 7, 0, false);

    // Draw the score
    DrawTextSmall("SCORE", SCORE_X, SCORE_Y, 0, false);
    DrawNumSmallLZ(score, 6, SCORE_X, SCORE_Y + 7, 0, false);
    
    // Cover piece out of bounds. Due to the sprite layer having a higher z-order than the tile layer,
    // we need to re-draw a few background tiles as sprites, so it appears as though part of the 
    // piece sprite is behind the background tiles, when the piece extends past the top of the play area.
    drawSprite(32, 8, 0xCD, 7);
    drawSprite(40, 8, 0xCC, 7);
    drawSprite(48, 8, 0xCC, 7);
    drawSprite(56, 8, 0xCC, 7);
    drawSprite(64, 8, 0xCC, 7);
    drawSprite(72, 8, 0xCC, 7);
    drawSprite(80, 8, 0xCE, 7);
    drawSprite(32, 16, 1, 0);
    drawSprite(40, 16, 2, 0);
    drawSprite(48, 16, 2, 0);
    drawSprite(56, 16, 2, 0);
    drawSprite(64, 16, 2, 0);
    drawSprite(72, 16, 2, 0);
    drawSprite(80, 16, 3, 0);

    // If the player is still playing, draw the locked pieces and the current active piece.
    // Otherwise, draw the 'game over' pieces on the board.
    if (igs == playing)
    {
        // Draw the locked pieces
        for (int i = 0; i < NUM_BLOCKS_X; i++) 
            for (int j = 0; j < NUM_BLOCKS_Y; j++) 
                if (board[i][j] > 0)
                    drawSprite((i*BLK_SIZE)+BOARD_X, (j*BLK_SIZE)+BOARD_Y, board[i][j], blockPalette);

        // Draw the active piece
        uint8_t px = (x * BLK_SIZE) + BOARD_X;
        uint8_t py = (y * BLK_SIZE) + BOARD_Y;
        DrawPiece(px, py, piece, rotation);
    }
    // If the in-game state is "dead", then the board has filled up, so we need to kick off the
    // kill animation and fill up the board with blocks.
    else if (igs == dead)
    {
        if (endAnimTimer == -1) {                           // If the end animation timer has not yet been started
            endAnimTimer = END_ANIM_TICKS;                  // Start the end animation sequence
            endAnimRowCount = 0;                            // Begin at the top row
            if (musicOn) 
                audio.StopTrack(selectedMusicOption);       // Kill the BG music
            if (sfxOn) audio.PlayTrack(SND_GAMEOVER);       // Play the kill sound
        } 
        else if (endAnimRowCount != NUM_BLOCKS_Y) {                         // End animation sequenxe has been started, but is not yet complete
            if (--endAnimTimer == 0 && endAnimRowCount < NUM_BLOCKS_Y) {    // If it's time to cover another row
                endAnimRowCount++;                                          // Cover the next row
                endAnimTimer = END_ANIM_TICKS;                              // Reset the timer
            }
        }
        // Cover the rows
        for (int i = 0; i < NUM_BLOCKS_X; i++)                  
            for (int j = 0; j < endAnimRowCount; j++)
                drawSprite((i*BLK_SIZE)+BOARD_X, (j*BLK_SIZE)+BOARD_Y, 0xCB, blockPalette);
    } 
    
    if (clearList[0] > -1 || clearList[1] > -1 || clearList[2] > -1 || clearList[3] > -1)
    {
        if (clearAnimTimer == -1) {
            if (sfxOn) {
                if (GetNumToClear() == 4) audio.PlayTrack(SND_CLEAR4);
                else audio.PlayTrack(SND_CLEAR);
            }
            clearAnimTimer = CLR_ANIM_TICKS;
            clearAnimBlock = 5;
        } else if (clearAnimBlock < NUM_BLOCKS_X) {
            if (--clearAnimTimer == 0) {
                if (clearList[0] > -1) {
                    board[clearAnimBlock][clearList[0]] = 0;
                    board[NUM_BLOCKS_X - clearAnimBlock - 1][clearList[0]] = 0;
                }
                if (clearList[1] > -1) {
                    board[clearAnimBlock][clearList[1]] = 0;
                    board[NUM_BLOCKS_X - clearAnimBlock - 1][clearList[1]] = 0;
                }
                if (clearList[2] > -1) {
                    board[clearAnimBlock][clearList[2]] = 0;
                    board[NUM_BLOCKS_X - clearAnimBlock - 1][clearList[2]] = 0;
                }
                if (clearList[3] > -1) {
                    board[clearAnimBlock][clearList[3]] = 0;
                    board[NUM_BLOCKS_X - clearAnimBlock - 1][clearList[3]] = 0;
                }
                clearAnimBlock++;
                clearAnimTimer = CLR_ANIM_TICKS;
            }
        } else {
            clearAnimTimer = -1;
            clearAnimBlock = 0;
            ClearLines();
            ResetClearList();
        }
    }

    if (IsGameOver())
    {
        igs = dead;                                     // Initiate the death sequence
        hiscore = score > hiscore ? score : hiscore;    // Set the new high score
    }
}

/*************************************************************************
* HandleButtons
* Checks and handles button events
*************************************************************************/
void HandleButtons()
{
    if (button(right_but)) {                        // Player pressing right button?
        if (CanMoveRight()) {                       // If piece can be moved...
            if (!rInSlide) {                        // If we're not in a slide
                x++;                                // Move right one space
                rInSlide = true;                    // Activate slide mode
                rSlideInitTimer = INIT_SLIDE_SPD;   // Start the initial slide timer
                rSlideTimer = SLIDE_SPD;            // Set the mid-slide speed
                if (sfxOn) audio.PlayTrack(SND_MOVE);
            } else {                                // Else, we're in a slide
                if (rSlideInitTimer == 0) {         // Initial slide timer is up
                    if (rSlideTimer == 0) {         // Mid-slide timer is up
                        x++;                        // Move the piece 
                        rSlideTimer = SLIDE_SPD;    // Reset the mid-slide timer
                        if (sfxOn) audio.PlayTrack(SND_MOVE);
                    }  else {                       // Mid-slide timer not up
                        rSlideTimer--;              // Decrement mid-slide timer
                    }
                } else {                            // Initial slide timer not up
                    rSlideInitTimer--;              // Decrement the initial slide timer
                }
            }
        }
    } else {                                        // Right button not pressed
        rInSlide = false;                           // Disable slide trigger
        rSlideTimer = 0;                            // Reset mid-slide timer
        rSlideInitTimer = 0;                        // Reset initial slide timer
    }

    if (button(left_but)) {                         // Player pressing left button?
        if (CanMoveLeft()) {                        // If piece can be moved...
            if (!lInSlide) {                        // If we're not in a slide
                x--;                                // Move left one space
                lInSlide = true;                    // Activate slide mode
                lSlideInitTimer = INIT_SLIDE_SPD;   // Start the initial slide timer
                lSlideTimer = SLIDE_SPD;            // Set the mid-slide speed
                if (sfxOn) audio.PlayTrack(SND_MOVE);
            } else {                                // Else, we're in a slide
                if (lSlideInitTimer == 0) {         // Initial slide timer is up
                    if (lSlideTimer == 0) {         // Mid-slide timer is up
                        x--;                        // Move the piece 
                        lSlideTimer = SLIDE_SPD;    // Reset the mid-slide timer
                        if (sfxOn) audio.PlayTrack(SND_MOVE);
                    }  else {                       // Mid-slide timer not up
                        lSlideTimer--;              // Decrement mid-slide timer
                    }
                } else {                            // Initial slide timer not up
                    lSlideInitTimer--;              // Decrement the initial slide timer
                }
            }
        }
    } else {                                        // Left button not pressed
        lInSlide = false;                           // Disable slide trigger
        lSlideTimer = 0;                            // Reset mid-slide timer
        lSlideInitTimer = 0;                        // Reset initial slide timer
    }

    if (button(up_but)) {                           // Up button doesn't do anything during the game
    }

    if (button(down_but)) {
        if (!softDropReset) {                       // Soft drop debounce trigger is off
            softDrop = SOFTDROP_SPD;                // Activate soft drop
        }
    } else {                                        // Player has let go of down button
        softDropReset = false;                      // Reset the soft drop debounce trigger
        softDrop = 0;                               // Turn off soft drop
        softDropFrames = 0;                         // Reset the soft drop frame counter
    }

    if (button(B_but)) {                            // If the player presses B
        if (CanRotate())                            // Check if the piece can be rotated
        {
            rotation = (rotation + 1) % 4;          // Rotate the piece clockwise
            if (sfxOn)                              // If sound effects are on...
                audio.PlayTrack(SND_ROTATE);        // Play the rotation sound
        }            
    }

    if (button(A_but)) {                            // If the player presses A
        if (CanRotate())                            // Check if the piece can be rotated
        {
            if (rotation > 0) rotation--;           // Rotate the piece counter-clockwise
            else rotation = 3;
            if (sfxOn)                              // If sound effects are on...
                audio.PlayTrack(SND_ROTATE);        // Play the rotation sound
        }
    }

    if (button(C_but)) {  }                         // C button doesn't do anything during game play

    if (button(start_but)) {                        // If the player presses START
        doPause = true;                             // Pause the game
    }

    if (button(select_but)) {                       // If the player presses SELECT
        hideNext = hideNext ? false : true;         // Hide the next piece
    }
}

/*************************************************************************
* IsBlockBelow
* Checks to see if there's a block below the one at the board location
* passed in. This function is used to determine if the piece can move
* down the board.
*************************************************************************/
bool IsBlockBelow(int8_t bx, int8_t by)
{
    if (by == 19) return true;
    if (by >= 0 && board[bx][by+1] > 0) return true;
    return false;
}

bool IsBlockLeft(int8_t bx, int8_t by)
{
    if (bx == 0) return true;
    if (by >= 0 && bx > 0 && board[bx - 1][by] > 0) return true;
    return false;
}

bool IsBlockAt(int8_t bx, int8_t by)
{
    if (by < 0) return false;
    if (bx < 0 || bx > NUM_BLOCKS_X - 1) return true;
    if (by > NUM_BLOCKS_Y - 1) return true;
    if (board[bx][by] > 0) return true;
    return false;
}

bool IsBlockRight(int8_t bx, int8_t by)
{
    if (bx == NUM_BLOCKS_X - 1) return true;
    if (by >= 0 && bx < NUM_BLOCKS_X - 1 && board[bx + 1][by] > 0) return true;
    return false;
}

bool CanMoveDown()
{
    bool canMove = true;
    switch (piece)
    {
        case PIECE_I:
            for (auto cell : piece_i[rotation])    
                if (IsBlockBelow(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_J:
            for (auto cell : piece_j[rotation])    
                if (IsBlockBelow(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_L:
            for (auto cell : piece_l[rotation])    
                if (IsBlockBelow(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_O:
            for (auto cell : piece_o[rotation])    
                if (IsBlockBelow(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_S:
            for (auto cell : piece_s[rotation])    
                if (IsBlockBelow(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_T:
            for (auto cell : piece_t[rotation])    
                if (IsBlockBelow(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_Z:
            for (auto cell : piece_z[rotation])    
                if (IsBlockBelow(x + cell.x, y + cell.y))
                    return false;
            break;
        default:
            break;
    }
    return canMove;
}

bool CanMoveLeft()
{
    bool canMove = true;
    switch (piece)
    {
        case PIECE_I:
            for (auto cell : piece_i[rotation])    
                if (IsBlockLeft(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_J:
            for (auto cell : piece_j[rotation])    
                if (IsBlockLeft(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_L:
            for (auto cell : piece_l[rotation])    
                if (IsBlockLeft(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_O:
            for (auto cell : piece_o[rotation])    
                if (IsBlockLeft(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_S:
            for (auto cell : piece_s[rotation])    
                if (IsBlockLeft(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_T:
            for (auto cell : piece_t[rotation])    
                if (IsBlockLeft(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_Z:
            for (auto cell : piece_z[rotation])    
                if (IsBlockLeft(x + cell.x, y + cell.y))
                    return false;
            break;
        default:
            break;
    }
    return canMove;
}

bool CanMoveRight()
{
    bool canMove = true;
    switch (piece)
    {
        case PIECE_I:
            for (auto cell : piece_i[rotation])    
                if (IsBlockRight(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_J:
            for (auto cell : piece_j[rotation])    
                if (IsBlockRight(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_L:
            for (auto cell : piece_l[rotation])    
                if (IsBlockRight(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_O:
            for (auto cell : piece_o[rotation])    
                if (IsBlockRight(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_S:
            for (auto cell : piece_s[rotation])    
                if (IsBlockRight(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_T:
            for (auto cell : piece_t[rotation])    
                if (IsBlockRight(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_Z:
            for (auto cell : piece_z[rotation])    
                if (IsBlockRight(x + cell.x, y + cell.y))
                    return false;
            break;
        default:
            break;
    }
    return canMove;
}

// Takes the screen x, y (upper left coordinate of the center block) and draws the
// tetromino there
void DrawPiece(uint8_t px, uint8_t py, uint8_t pieceType, uint8_t rot)
{
    switch (pieceType)
    {
    case PIECE_I:
        for (auto cell : piece_i[rotation])    
            drawSprite(px + (BLK_SIZE * cell.x), py + (BLK_SIZE * cell.y), 0x09, blockPalette);
        break;
    case PIECE_J:
        for (auto cell : piece_j[rotation])    
            drawSprite(px + (BLK_SIZE * cell.x), py + (BLK_SIZE * cell.y), 0x0A, blockPalette);
        break;
    case PIECE_L:
        for (auto cell : piece_l[rotation])    
            drawSprite(px + (BLK_SIZE * cell.x), py + (BLK_SIZE * cell.y), 0x0B, blockPalette);
        break;
    case PIECE_O:
        for (auto cell : piece_o[rotation])    
            drawSprite(px + (BLK_SIZE * cell.x), py + (BLK_SIZE * cell.y), 0x09, blockPalette);
        break;
    case PIECE_S:
        for (auto cell : piece_s[rotation])    
            drawSprite(px + (BLK_SIZE * cell.x), py + (BLK_SIZE * cell.y), 0x0A, blockPalette);
        break;
    case PIECE_T:
        for (auto cell : piece_t[rotation])    
            drawSprite(px + (BLK_SIZE * cell.x), py + (BLK_SIZE * cell.y), 0x09, blockPalette);
        break;
    case PIECE_Z:
        for (auto cell : piece_z[rotation])    
            drawSprite(px + (BLK_SIZE * cell.x), py + (BLK_SIZE * cell.y), 0x0B, blockPalette);
        break;
    default:
        break;
    }
}

void DrawSmallPiece(uint8_t px, uint8_t py, uint8_t pieceType)
{
    switch (pieceType)
    {
    case PIECE_I:
        for (auto cell : piece_i[ROT_0])    
            drawSprite(px + (SM_BLK_SIZE * cell.x), py + (SM_BLK_SIZE * cell.y), 0xC8, blockPalette);
        break;
    case PIECE_J:
        for (auto cell : piece_j[ROT_0])    
            drawSprite(px + (SM_BLK_SIZE * cell.x) - SM_BLK_SIZE / 2, py + (SM_BLK_SIZE * cell.y), 0xC9, blockPalette);
        break;
    case PIECE_L:
        for (auto cell : piece_l[ROT_0])    
            drawSprite(px + (SM_BLK_SIZE * cell.x) - SM_BLK_SIZE / 2, py + (SM_BLK_SIZE * cell.y), 0xCA, blockPalette);
        break;
    case PIECE_O:
        for (auto cell : piece_o[ROT_0])    
            drawSprite(px + (SM_BLK_SIZE * cell.x), py + (SM_BLK_SIZE * cell.y), 0xC8, blockPalette);
        break;
    case PIECE_S:
        for (auto cell : piece_s[ROT_0])    
            drawSprite(px + (SM_BLK_SIZE * cell.x) - SM_BLK_SIZE / 2, py + (SM_BLK_SIZE * cell.y), 0xC9, blockPalette);
        break;
    case PIECE_T:
        for (auto cell : piece_t[ROT_0])    
            drawSprite(px + (SM_BLK_SIZE * cell.x) - SM_BLK_SIZE / 2, py + (SM_BLK_SIZE * cell.y), 0xC8, blockPalette);
        break;
    case PIECE_Z:
        for (auto cell : piece_z[ROT_0])    
            drawSprite(px + (SM_BLK_SIZE * cell.x) - SM_BLK_SIZE / 2, py + (SM_BLK_SIZE * cell.y), 0xCA, blockPalette);
        break;
    default:
        break;
    }
}

void FreezeBlock()
{
    switch (piece)
    {
        case PIECE_I:
            for (auto cell : piece_i[rotation])
                board[x + cell.x][y + cell.y] = 0x09;
            break;
        case PIECE_J:
            for (auto cell : piece_j[rotation])
                board[x + cell.x][y + cell.y] = 0x0A;
            break;
        case PIECE_L:
            for (auto cell : piece_l[rotation])
                board[x + cell.x][y + cell.y] = 0x0B;
            break;
        case PIECE_O:
            for (auto cell : piece_o[rotation])
                board[x + cell.x][y + cell.y] = 0x09;
            break;
        case PIECE_S:
            for (auto cell : piece_s[rotation])
                board[x + cell.x][y + cell.y] = 0x0A;
            break;
        case PIECE_T:
            for (auto cell : piece_t[rotation])
                board[x + cell.x][y + cell.y] = 0x09;
            break;
        case PIECE_Z:
            for (auto cell : piece_z[rotation])
                board[x + cell.x][y + cell.y] = 0x0B;
            break;
        default:
            break;
    }
}

bool CanRotate()
{
    switch (piece)
    {
        case PIECE_I:
            for (auto cell : piece_i[(rotation+1)%4])
                if (IsBlockAt(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_J:
            for (auto cell : piece_j[(rotation+1)%4])
                if (IsBlockAt(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_L:
            for (auto cell : piece_l[(rotation+1)%4])
                if (IsBlockAt(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_O:
            for (auto cell : piece_o[(rotation+1)%4])
                if (IsBlockAt(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_S:
            for (auto cell : piece_s[(rotation+1)%4])
                if (IsBlockAt(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_T:
            for (auto cell : piece_t[(rotation+1)%4])
                if (IsBlockAt(x + cell.x, y + cell.y))
                    return false;
            break;
        case PIECE_Z:
            for (auto cell : piece_z[(rotation+1)%4])
                if (IsBlockAt(x + cell.x, y + cell.y))
                    return false;
            break;
        default:
            break;
    }
    return true;
}

void ClearLines()
{
    uint8_t clears = 0;

    for (uint8_t j = 0; j < NUM_BLOCKS_Y; j++) {
        if (IsInClearList(j) && j != 0) {                       // If the row needs to be cleared
            for (uint8_t k = j; k > 0; k--) {                   // Drop the lines above it
                for (uint8_t i = 0; i < NUM_BLOCKS_X; i++) {    // Loop through each block on the line
                    board[i][k] = board[i][k-1];                // Set it to the block above it
                }
            }
            for (int i = 0; i < NUM_BLOCKS_X; i++) board[i][0] = 0;
            lines++;            // Increase line counter
            SetLevel();         // Set the appropriate level
            clears++;           // Increment # of lines cleared in this check
        } else if (IsInClearList(j) && j == 0) {
            // Zero out the blocks in the line
            for (int i = 0; i < NUM_BLOCKS_X; i++) board[i][j] = 0;

            lines++;            // Increase line counter
            SetLevel();         // Set the appropriate level
            clears++;           // Increment # of lines cleared in this check
        }
    }
    
    score += CalcScore(clears, level);      // Increase score
}

void BuildClearList()
{
    for (uint8_t j = 0; j < NUM_BLOCKS_Y; j++)
    {
        bool cleared = true;
        for (uint8_t i = 0; i < NUM_BLOCKS_X; i++)
        {
            if (board[i][j] == 0) cleared = false;
        }
        if (cleared) {
            if (clearList[0] == -1) clearList[0] = j;
            else if (clearList[1] == -1) clearList[1] = j;
            else if (clearList[2] == -1) clearList[2] = j;
            else clearList[3] = j;
        }
    }
}

void ResetClearList()
{
    clearList[0] = -1;
    clearList[1] = -1;
    clearList[2] = -1;
    clearList[3] = -1;
}

uint8_t GetNumToClear()
{
    uint8_t num = 0;
    if (clearList[0] > -1) num++;
    if (clearList[1] > -1) num++;
    if (clearList[2] > -1) num++;
    if (clearList[3] > -1) num++;
    return num;
}

bool IsInClearList(uint8_t row)
{
    if (clearList[0] == row) return true;
    if (clearList[1] == row) return true;
    if (clearList[2] == row) return true;
    if (clearList[3] == row) return true;
    return false;
}

bool IsGameOver()
{
    for (uint8_t col = 0; col < NUM_BLOCKS_X; col++)
        if (board[col][0] > 0)
            return true;
    
    return false;
}

uint8_t GenerateRandomPiece()
{
    return random(7);
}

void InitializeGame()
{
    // Clear out all the spaces on the game board
    for (uint8_t row = 0; row < NUM_BLOCKS_X; row++)
        for (uint8_t col = 0; col < NUM_BLOCKS_Y; col++)
            board[row][col] = 0;

    // Reset the piece stats
    for (uint8_t p; p < 7; p++)
        stats[p] = 0;

    // Reset the player name
    for (uint8_t n; n < 6; n++)
        name[n] = '-'; 

    score = 0;                          // Reset the score
    lines = 0;                          // Reset the line counter
    rotation = ROT_0;                   // Reset piece rotation
    next = GenerateRandomPiece();       // Generate a random next piece
    piece = GenerateRandomPiece();      // Generate a new piece
    stats[piece]++;                     // Update piece counter for initial piece
    SetBlockPalette();                  // Set the correct block colors, based on the level
    UpdateDropSpeed();                  // Reset the speed
    softDropFrames = 0;                 // Frame counter for soft drop
    softDrop = 0;                       // Reset the soft drop speed
    softDropReset = false;              // Reset the soft drop debounce trigger
    lockDelay = 0;                      // Reset the lock delay timer
    prevLockDelay = false;              // Reset the lock delay previous state
    x = START_X_BL;                     // Set the starting x grid cell
    y = START_Y_BL;                     // Set the starting y grid cell
    lockBypass = 0;                     // Reset the lock delay bypass frame counter
    igs = playing;                      // Reset in-game state
    endAnimTimer = -1;                  // Reset the end-game animation timer
    endAnimRowCount = 0;                // Reset the end-game animation row counter
    clearAnimTimer = -1;                // Reset the row clear animation timer
    clearAnimBlock = 0;                 // Reset the row clear animation block counter
    ResetClearList();                   // Reset the clear list
    hideNext = false;                   // Reset the next piece hidden option
    currentChar = 0;                    // Reset name entry character number
    hiscore = hiScoreTable[0].score;    // Load the Top Score displayed in game
}

void UpdateDropSpeed()
{
    if (level < 9) speed = 24 - ceil(5*level/2);
    else if (level >= 9 && level <=12)  speed = 3;
    else if (level >= 13 && level <= 18) speed = 2;
    else speed = 1;
}

void SetLevel()
{
    if (lines / 10 > level) {
        level++;
        audio.PlayTrack(SND_LEVELUP);
        SetBlockPalette();
        UpdateDropSpeed();
    }
}

void SetBlockPalette()
{
    if (level > 5 && level < 10) loadPalette("badgeris/badgeris2.dat");
    else loadPalette("badgeris/badgeris1.dat");

    blockPalette = (level % 5) + 1;
}

uint32_t CalcScore(uint8_t lines, uint8_t level)
{
    if (lines == 0 || lines > 4) return 0;

    const uint16_t m[4] = { 40, 100, 300, 1200 };
    return m[lines - 1] * (level + 1);
}

void SplashLoop() { 
    // Buttons
    if (button(start_but)) {
        ChangeState(titleScreen);
    }

    // Draw
    DrawTextSmall("BADGERIS", 60, 17, 2, true);
    DrawTextSmall("GAMEBADGE3 TETRIS REMAKE", 60, 37, 0, true);
    DrawTextSmall("   KEN ST. CYR", 60, 47, 1, true);
    DrawTextSmall("BY            ", 60, 47, 0, true);
    DrawTextSmall("WHATSKENMAKING.COM", 60, 57, 2, true);
    DrawTextSmall("ORIGINAL CONCEPT, DESIGN", 60, 77, 0, true);
    DrawTextSmall("AND PROGRAM", 60, 87, 0, true);
    DrawTextSmall("   ALEXEY PAZHITNOV", 60, 97, 1, true);
    DrawTextSmall("BY                 ", 60, 97, 0, true);
}

void TitleLoop() { 
    // Buttons
    if (button(start_but)) {
        ChangeState(optionsScreen);
    }

    for (uint8_t i = 0; i < 13; i++) {                              // BADGERIS title text
        drawSprite(11 + i * 8, 26, 0x30 + i, 1);
        drawSprite(11 + i * 8, 34, 0x40 + i, 1);
        drawSprite(11 + i * 8, 36, 0x50 + i, 2);
        drawSprite(11 + i * 8, 44, 0x60 + i, 2);
    }
    for (uint8_t i = 0; i < 3; i++) {                               // gameBadge3 Sprite
        for(uint8_t j = 0; j < 4; j++) {
            drawSprite(75 + i * 8, 62 + j * 8, 0x3D + i + j * 16, 1);
        }
    }
    DrawTextSmall("PRESS START", 15, 76, 1, false);                 // PRESS START

}

void CongratsLoop() {
    static uint8_t cursorCounter = 0;
    static bool cursorOn;
    static uint8_t y = 72;
    static uint8_t charIndex = 0;
    static uint8_t charTicks = 0;

    // Buttons
    if (button(start_but)) {
        if (CheckHighScore(score) > -1) {
            UpdateHighScoreTable(ScoreEntry{String(name), score, level});
            SaveHighScoreTable();
        }
        audio.StopTrack(SND_HISCORE);
        ChangeState(optionsScreen);
    }
    if (button(up_but) || button(down_but)) {
        if (charTicks == 0)
        {
            if (button(up_but)) {
                charIndex = charIndex < sizeof(alphaSequence) - 1 ? charIndex + 1 : 0;
            } else {
                charIndex = charIndex > 0 ? charIndex - 1 : sizeof(alphaSequence) - 1;
            }

            name[currentChar] = alphaSequence[charIndex];
            audio.PlayTrack(SND_MOVE);
        }
        charTicks = (charTicks + 1) % CHAR_TICK_RATE;
    } else {
        charTicks = 0;    // Reset the char timer if the player isn't pressing up or down
    }

    if (button(B_but) || button(right_but)) {
        audio.PlayTrack(SND_MOVE);
        currentChar = currentChar < 5 ? currentChar + 1 : 0;

        for (uint8_t i = 0; i < sizeof(alphaSequence); i++)
            if (alphaSequence[i] == name[currentChar]) {
                charIndex = i;
                break;
            }
    }

    if (button (A_but) || button(left_but)) {
        audio.PlayTrack(SND_MOVE);
        currentChar = currentChar > 0 ? currentChar - 1 : 5;
        
        for (uint8_t i = 0; i < sizeof(alphaSequence); i++)
            if (alphaSequence[i] == name[currentChar]) {
                charIndex = i;
                break;
            }
    }

    int8_t scorepos = CheckHighScore(score);  

    DrawTextSmall("CONGRATULATIONS", 60, 14, 0, true);
    DrawTextSmall("YOU ARE A", 60, 26, 0, true);
    DrawTextSmall("BADGERIS MASTER.", 60, 34, 0, true);
    DrawTextSmall("ENTER YOUR NAME", 60, 46, 0, true);
    DrawTextSmall(" NAME  SCORE  LV", 24, 60, 0, false);

    if (scorepos >= 0) {
        for (uint8_t i = 0; i < scorepos; i++) {
            DrawTextSmall(intToStringWithLeadingZeros(i+1, 1).c_str(), 16, y + i * 9, 0, false);
            DrawTextSmall(hiScoreTable[i].name.c_str(), 24, y + i * 9, 0, false);
            DrawTextSmall(intToStringWithLeadingZeros(hiScoreTable[i].score, 6).c_str(), 59, y + i * 9, 0, false);
            DrawTextSmall(intToStringWithLeadingZeros(hiScoreTable[i].level, 2).c_str(), 94, y + i * 9, 0, false); 
        }

        for (uint8_t i = scorepos; i < 3; i++) {
            DrawTextSmall(intToStringWithLeadingZeros(i+1, 1).c_str(), 16, y + i * 9, 0, false);
            if (i == scorepos) {
                DrawTextSmall(String(name).c_str(), 24, y + i * 9, 0, false);
                DrawTextSmall(intToStringWithLeadingZeros(score, 6).c_str(), 59, y + i * 9, 0, false);
                DrawTextSmall(intToStringWithLeadingZeros(level, 2).c_str(), 94, y + i * 9, 0, false); 
            } else {
                DrawTextSmall(hiScoreTable[i-1].name.c_str(), 24, y + i * 9, 0, false);
                DrawTextSmall(intToStringWithLeadingZeros(hiScoreTable[i-1].score, 6).c_str(), 59, y + i * 9, 0, false);
                DrawTextSmall(intToStringWithLeadingZeros(hiScoreTable[i-1].level, 2).c_str(), 94, y + i * 9, 0, false); 
            }
        }
    } else {
        for (uint8_t i = 0; i < 3; i++) {
            DrawTextSmall(intToStringWithLeadingZeros(i+1, 1).c_str(), 16, y + i * 9, 0, false);
            DrawTextSmall(hiScoreTable[i].name.c_str(), 24, y + i * 9, 0, false);
            DrawTextSmall(intToStringWithLeadingZeros(hiScoreTable[i].score, 6).c_str(), 59, y + i * 9, 0, false);
            DrawTextSmall(intToStringWithLeadingZeros(hiScoreTable[i].level, 2).c_str(), 94, y + i * 9, 0, false); 
        }
    }
    
    
    if (++cursorCounter >= CUR_BLINK_RATE) {
        cursorOn = !cursorOn;
        cursorCounter = 0;
    }
    if (scorepos > -1 && cursorOn) {
        // Draw blinking cursor over name
        uint8_t cursorx = currentChar * 5 + 24;
        uint8_t cursory = scorepos * 9 + y;
        drawSprite(cursorx, cursory, 0xEE, 0);
    }
}

void LevelLoop() {
    static uint8_t y = 72;
    static int8_t l = 0;               // Index of the selected level

    // Buttons
    if (button(start_but)) {
        level = l;
        ChangeState(gameScreen);
    }
    if (button(right_but)) { 
        l = l < 9 ? l + 1 : 0;
        audio.PlayTrack(SND_MOVE);
    }
    if (button(left_but)) {
        l = l > 0 ? l - 1 : 9;
        audio.PlayTrack(SND_MOVE);
    }
    if (button(down_but)) {
        l = l < 5 ? l + 5 : l;
        audio.PlayTrack(SND_MOVE);
    }
    if (button(up_but)) {
        l = l > 4 ? l - 5 : l;
        audio.PlayTrack(SND_MOVE);
    }
    
    // Draw the Level Select Options
    DrawTextSmall("SELECT A LEVEL", 60, 24, 0, true);
    for (uint8_t i = 0; i < 10; i++) {
        DrawTextSmall(String(i).c_str(), 42 + ((i % 5) << 3), 33 + ((i / 5) << 3), 0, false);
    }

    drawSprite(40 + ((l % 5) << 3), 32 + ((l / 5) << 3), 0x73, 5);          // Draw the cursor
    
    // Draw the high score table
    DrawTextSmall(" NAME  SCORE  LV", 24, 60, 0, false);
    for (uint8_t i = 0; i < 3; i++) {
        DrawTextSmall(intToStringWithLeadingZeros(i+1, 1).c_str(), 16, y + i * 9, 0, false);
        DrawTextSmall(hiScoreTable[i].name.c_str(), 24, y + i * 9, 0, false);
        DrawTextSmall(intToStringWithLeadingZeros(hiScoreTable[i].score, 6).c_str(), 59, y + i * 9, 0, false);
        DrawTextSmall(intToStringWithLeadingZeros(hiScoreTable[i].level, 2).c_str(), 94, y + i * 9, 0, false); 
    }
}

void PauseLoop() { 
    // Buttons
    if (button(start_but)) {
        ChangeState(gameScreen);
    }
    if (button(select_but)) {
        musicOn = !musicOn;
        if (musicOn) {
            audio.PlayTrack(selectedMusicOption, true);
        } 
    }

    // Draw
    DrawTextSmall("PAUSE", 60, 60, 2, true);
}

void OptionsLoop() {
    
    // Buttons
    if (button(start_but)) {
        if (selectedMusicOption == 3) {
            selectedMusicOption = 0;
            musicOn = false;
        } else {
            audio.StopTrack(selectedMusicOption);               // Stop playing the music before entering the options screen
        }
        ChangeState(levelScreen);
    }
    if (button(down_but)) { 
        if (selectedMusicOption != 3) {
            audio.StopTrack(selectedMusicOption);
        }
        audio.PlayTrack(SND_MOVE);  // Play the tick sound
        selectedMusicOption = (selectedMusicOption + 1) % 4;
        if (selectedMusicOption != 3) {
            audio.PlayTrack(selectedMusicOption, true);
        }
    }
    if (button(up_but)) {
        if (selectedMusicOption != 3) {
            audio.StopTrack(selectedMusicOption);
        }
        audio.PlayTrack(SND_MOVE);  // Play the tick sound
        selectedMusicOption = selectedMusicOption ? selectedMusicOption - 1 : 3;  // Update selected music option
        if (selectedMusicOption != 3) {
            audio.PlayTrack(selectedMusicOption, true);
        }
    }
    if (button(right_but)) { 
        audio.PlayTrack(SND_MOVE);
        sfxOn = !sfxOn; 
    }
    if (button(left_but)) { 
        audio.PlayTrack(SND_MOVE);
        sfxOn = !sfxOn; 
    }

    // Draw
    DrawTextSmall("MUSIC TYPE", 12, 12, 0, false);
    DrawTextSmall("MUSIC-1", 84, 36, 0, true);
    DrawTextSmall("MUSIC-2", 84, 46, 0, true);
    DrawTextSmall("MUSIC-3", 84, 56, 0, true);
    DrawTextSmall("OFF", 84, 66, 0, true);
    DrawTextSmall("SFX", 12, 92, 0, false);
    DrawTextSmall("ON", 51, 92, 0, false);
    DrawTextSmall("OFF", 90, 92, 0, false);
    drawSprite(60, selectedMusicOption * 10 + 35, 0xEC, 0);
    drawSprite(102, selectedMusicOption * 10 + 35, 0xED, 0);
    drawSprite(85 - (sfxOn * 41), 91, 0xEC, 0);
}

void ChangeState(gamestate newstate)
{
    pauseLCD(true);
    lastState = currentState;                               // Save the previous screen state
    switch (newstate)
    {
        case splashScreen:                                  // Entering the Splash Screen (with the text)
            loadPalette("badgeris/splash.dat");             // Load the palette table used for the splash screen
            LoadTilePage(0, 0, ALLBLACK);                   // Paint an all black tile background
            break;
        case titleScreen:                                   // Entering the Title Screen
            loadPalette("badgeris/title.dat");              // Load the palette table used for the title screen
            LoadTilePage(0, 0, TITLE);                      // Paint the title screen tiles background
            break;
        case levelScreen:                                   // Entering the Level Select Screen
            setButtonDebounce(down_but, true, 1);
            setButtonDebounce(right_but, true, 1);
            setButtonDebounce(left_but, true, 1);
            setButtonDebounce(up_but, true, 1);
            loadPalette("badgeris/splash.dat");             // Load the palette table used for the Level Select Screen
            LoadTilePage(0, 0, LEVEL);                      // Paint the Level Select Screen background tiles
            break;
        case optionsScreen:
            setButtonDebounce(down_but, true, 1);
            setButtonDebounce(right_but, true, 1);
            setButtonDebounce(left_but, true, 1);
            setButtonDebounce(up_but, true, 1);
            loadPalette("badgeris/badgeris1.dat");
            LoadTilePage(0, 0, OPTIONS);
            audio.PlayTrack(selectedMusicOption, true);
            break;
        case gameScreen:
            setButtonDebounce(down_but, false, 1);
            setButtonDebounce(right_but, false, 1);
            setButtonDebounce(left_but, false, 1);
            SetBlockPalette();
            LoadTilePage(0, 0, BGTILES);
            if (lastState == levelScreen) {
                InitializeGame();                           // Reinit the game
            }
            if (musicOn) {
                if (lastState == pauseScreen)
                    audio.PauseTrack(selectedMusicOption);
                else
                    audio.PlayTrack(selectedMusicOption, true);
            }
            break;
        case pauseScreen:
            loadPalette("badgeris/splash.dat");
            LoadTilePage(0, 0, ALLBLACK);
            if (musicOn) {
                audio.PauseTrack(selectedMusicOption);                   // Pause the music during the pause screen
            }
            audio.PlayTrack(SND_PAUSE);
            break;
        case congratsScreen:
            setButtonDebounce(down_but, false, 1);
            setButtonDebounce(up_but, false, 1);
            setButtonDebounce(right_but, true, 1);
            setButtonDebounce(left_but, true, 1);
            loadPalette("badgeris/splash.dat");
            LoadTilePage(0, 0, CONGRAT);
            audio.PlayTrack(SND_HISCORE, true);
            break;
        default:
            break;
    }
    currentState = newstate;                                // Set the current state
    pauseLCD(false);
}

String intToStringWithLeadingZeros(int value, int digits) {
  String stringValue = String(value);
  while (stringValue.length() < digits) {
    stringValue = "0" + stringValue;
  }
  return stringValue;
}

void DrawTextSmall(const char *text, uint8_t x, uint8_t y, int whatPalette, bool centered)
{
    if (centered) {
        x -= (strlen(text) * 5) / 2;
    }

	while(*text) {
        char c = *text++ + 96;
		drawSprite(x, y, c, whatPalette);
		x += 5;
	}
}

void DrawNumSmallLZ(int num, int digits, uint8_t x, uint8_t y, uint8_t palette, bool centered)
{
    String s = intToStringWithLeadingZeros(num, digits);
    DrawTextSmall(s.c_str(), x, y, palette, centered);
}

int8_t CheckHighScore(int newscore) {
    if (newscore > hiScoreTable[0].score) return 0;
    if (newscore > hiScoreTable[1].score) return 1;
    if (newscore > hiScoreTable[2].score) return 2;
    return -1;
}

void UpdateHighScoreTable(ScoreEntry se)
{
    if (se.score > hiScoreTable[0].score) {
        hiScoreTable[2] = hiScoreTable[1];
        hiScoreTable[1] = hiScoreTable[0];
        hiScoreTable[0] = se;
        return;
    }
    if (se.score > hiScoreTable[1].score) {
        hiScoreTable[2] = hiScoreTable[1];
        hiScoreTable[1] = se;
        return;
    }
    if (se.score > hiScoreTable[2].score) {
        hiScoreTable[2] = se;
        return;
    }
}

void InitHighScoreTable()
{
    EEPROM.begin(256);
    if (EEPROM[0] != 0x8F) {
        SaveHighScoreTable();
    }
}

void SaveHighScoreTable()
{
    EEPROM.begin(256);
    for (uint8_t i = 0; i < 34; i++)                                // Zero out the existing high-score entries
        EEPROM.write(i, 0);
    EEPROM.write(0, 0x8F);                                          // First byte = 0x8F to indicate there's data saved
    for (uint8_t n = 0; n < 3; n++) {                               // Loop each entry in the high score table
        for (uint8_t i = 0; i < 6; i++) {                           
            EEPROM.write(n * 11 + i + 1, hiScoreTable[n].name[i]);      // Bytes 1 - 6: Player name (6 bytes)
        }
        EEPROM.put(n * 11 + 7, hiScoreTable[n].score);            // Bytes 7 - 10: Score (32-bit uint)
        EEPROM.write(n * 11 + 11, hiScoreTable[n].level);           // Byte 11: Level (8-bit uint)
    }
    EEPROM.end();
}

void ReadHighScoreTable()
{
    EEPROM.begin(256);
    char name[7] = "";
    if (EEPROM[0] == 0x8F) {
        for (uint8_t n = 0; n < 3; n++) {                           // Loop each entry in the high score table
            for (uint8_t i = 0; i < 6; i++) {                           
                name[i] = EEPROM.read(n * 11 + i + 1);                  // Bytes 1 - 6: Player name (6 bytes)
            }
            hiScoreTable[n].name = String(name);
            EEPROM.get(n * 11 + 7, hiScoreTable[n].score);        // Bytes 7 - 10: Score (32-bit uint)
            hiScoreTable[n].level = EEPROM.read(n * 11 + 11);       // Byte 11: Level (8-bit uint)
        }
    }
    EEPROM.end();
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