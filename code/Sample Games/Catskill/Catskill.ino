#include <gameBadgePico.h>
#include <GBAudio.h>
#include "gameObject.h"
#include "music.h"
//Your defines here------------------------------------

GBAudio music;

char testX = 0;

bool firstFrame = false;
bool displayPauseState = true;

bool paused = true;											//Player pause, also you can set pause=true to pause the display while loading files/between levels
volatile int nextFrameFlag = 0;								//The 30Hz IRS incs this, Core0 Loop responds when it reaches 2 or greater
bool drawFrameFlag = false;									//When Core0 Loop responses to nextFrameFlag, it sets this flag so Core1 will know to render a frame
bool frameDrawing = false;									//Set TRUE when Core1 is drawing the display. Core0 should wait for this to clear before accessing video memory

//Your game variables here:
int xPos = 0;
int yPos = 0;
int dir = 0;

int dutyOut;

enum movement { rest, starting, walking, running, jumping, moving, turning, swiping, stopping, entering, exiting, damaged, dead };

movement budState = rest;

int entryWindow;
int budSpawnTimer = 0;
bool budSpawned = false;
int budFrame = 0;
int budSubFrame = 0;
bool budDir = false;	//True = going left
int budX = 56;
int budY = 8;

int budSwipe = 99;							//0 = not swiping, 1 2 3 swipe cycle

int16_t budWorldX = 8 * (6 + 7);			//Bud's position in the world (not the same as screen or tilemap)
int16_t budWorldY = 8;

uint16_t budWx1, budWx2, budWy1, budWy2;	//Upper left and lower right edge of Bud's hit box. It changes depending on what he's doing

uint16_t budAx1, budAx2, budAy1, budAy2;	//Upper left and lower right edge of Bud's attack hit box



int tail = 0;
int blink = 0;

int jump = 0;
bool moveJump = false;
int velocikitten = 0;

#define levelWidth			512

uint16_t level[levelWidth];
uint16_t xStripDrawing = 0;					//Which strip we're currently drawing

int16_t worldX = 6 * 8;					//Where the camera is positioned in the world (not the same as tilemap as that's dynamic)
int16_t worldY = 0;

int xWindowFine = 0;						//The fine scrolling amount
int8_t xWindowCoarse = 6;				//In the tilemap, the left edge of the current positiom (coarse)
int8_t xWindowBudFine = 0;
int8_t xWindowBudToTile = 6 + 7;		//Where Bud is (on center of screen, needs to be dynamic)
int8_t xStripLeft = 0;				    //Strip redraw position on left (when moving left)
int8_t xStripRight = 26;					//Strip redraw position on right (when moving right)

int16_t xLevelCoarse = 6;				//In the tilemap, the left edge of the current positiom (coarse)
int16_t xLevelStripLeft = 0;			//Strip redraw position on left (when moving left)
int16_t xLevelStripRight = 26;			//Strip redraw position on right (when moving right)

//Define flags for tiles. Use setTileType() to add these to tiles after you've drawn them
#define tilePlatform		0x80
#define tileBlocked			0x40

int tileDropAttribute = 2;				//2 = 0x80 platform 3 = 0x40 blocked
int tileDropAttributeMask = 0x80;			//0x80 platform 0x40 blocked


#define hallBlank				0
#define hallCounterWindow		1
#define hallCounter				2
#define hallLeftWall			3
#define hallRightWall			4
#define hallElevator			5
#define hallCallButton			6
#define hallDoorClosed			10
#define hallDoorOpen			11
#define hallWindow				12
#define hallGlass				13

#define bigRobot			1
#define chandelier			2
#define chandelierSmashed 	3
#define roomba				10
#define greenie				255

bool chandelierFalling = false;
int cWF = 0xFF;						//Stands for chandelier which falling it's short so functions don't get insane

int currentFloor = 1;
int currentCondo = 0;				//Which condo we've selected

int doorTilePos[6] = { 10, 28, 46, 86, 104, 122 };

//0x80 = Apartment cleared 0x40 = Door open, ready to enter. If cleared number is changed to XX and can't be re-opened
char apartmentState[6] = { 0, 0, 0, 0, 0, 0 };	//Zero index not used, starts at 1 goes to 6

bool elevatorOpen = false;

int inArrowFrame = 0;

#define maxThings		128
//thingObject thing[maxThings];

gameObject object[maxThings];

int totalThings = 0;
int lastRemoved = 0xFF;
int lastAdded = 0xFF;

bool gameActive = false;
bool isDrawn = false;													//Flag that tells new state it needs to draw itself before running logic

enum stateMachineGame { bootingMenu, splashScreen, titleScreen, diffSelect, levelEdit, saveGame, loadGame, game, story, goHallway, goCondo, goJail, goElevator, gameOver, pauseMode, theEnd};					//State machine of the game (under stateMachine = game)
stateMachineGame gameState = bootingMenu;

enum stateMachineEdit { tileDrop, tileSelect, objectDrop, objectSelect };					//State machine of the game (under stateMachine = game)
stateMachineEdit editState = tileDrop;


uint16_t menuTimer = 0;
uint8_t cursorTimer;

uint8_t cursorX, cursorY, cursorAnimate;

uint8_t soundTimer = 0;


#define condoWidth		120  				//15 * 8
#define hallwayWidth 	136					//Lopsided, it's based on doors and dressers also symmetrical

//static uint16_t condoMap[15][condoWidth];
static uint16_t condoMap[15][140];				//Condo is 120 tiles wide, hallway is 136 tiles wide so 140 covers both

int mapWidth = hallwayWidth;					//The width of the currently loaded map. Sets where wraparound happens (like condoWidth)

bool drawFine = false;		//Fine control for object drop on/off
int drawXfine = 7 * 8;
int drawYfine = 7 * 8;
int drawX = 7;				//Cursor XY coarse
int drawY = 7;
int cursorBlink	= 0;			//Cursor blinking
bool cursorMenuShow = false;	//True = show selection stuff
int cursorDrops = 0;			//0 = tiles, 1 = tile attributes, 2 = sprites
int cursorMenu = 0;				//0 = tiles 1 = sprites
bool editEntryFlag = false;		//Use to debounce A when entering edit mode
int cursorMoveTimer = 0;		//During editing, if D-pad held for X frames we move full speed
bool bButtonFlag = false;		//Changes debounce between tile drop and tile select

//The tile select window
int tileSelectX = 0;		//+2 = tile to drop
int tileSelectY = 0;		//+2 = tile to drop
int nextTileDrop = 0xA0;		//Place this value after selecting tile
int tilePlacePalette = 1;	//What palette dropping tiles use (0-3)

int editWindowX = 0;		//The editing window position on the condo map

bool editType = true;				//True = condo False = Hallway

int currentCopyBuffer = 0;
uint16_t copyBuffer[15][60];			//Edit buffer holds up to 4 screens (4 paste buffers)
int copyBufferUL[4][2];				//The upper left corner of what we copied
int copyBufferLR[4][2];				//The lower right corner of what we copied
int copyBufferSize[4][2];			//[num][0=x size 1=y size] of each buffer

int copySelectWhich = 0;				//0 = inactive, 1 = select upper left, 2 = select lower right

int editAaction = 0;					//What the A button does in edit mode
bool manualAdebounce = false;

bool copyMode = false;
bool pasteMode = false;
bool editMenuActive = false;
int editMenuSelectionY = 0;
int editMenuBaseY = 0;

char message[15];					    //A message on bottom of edit screen?
int messageTimer = 0;					//If above zero, dec per frame and erase menu once zero

char mapFileName[20] = { 'l', 'e', 'v', 'e', 'l', 's', '/', 'c', 'o', 'n', 'd', 'o', '1', '-', '1', '.', 'm', 'a', 'p', 0 };	//levels/condoX_X.map + zero terminator

char fileNameFloor = '1';
char fileNameCondo = '1';

//0= tall robot 1=can robot, 2 = flat robot, 3= dome robot, 4= kitten, 5= greenie (greenies on bud sheet 1)
int robotSheetIndex[6][2] = { {0, 0}, {0, 8}, {0, 12}, {8, 0}, {8, 5}, {6, 3}  };

int robotTypeSize[6][2] = { {2, 6}, {3, 3}, { 4, 2 }, { 4, 3 }, { 2, 2 }, {1, 1} };		//Size index
int selectRobot = 0;

uint16_t sentryLeft = 0;				//Left sentry edge of next object
uint16_t sentryRight = 120;

int objectSheetIndex[5][2];				//Stores the XY sprite sheet index of current object)
int objectTypeSize[5][2] = { {0, 0}, {4, 2}, { 2, 2 }, { 2, 3 }, { 3, 2 } };		//Size index (category 0 is variable, rest are static)
int objectTypeRange[5][2] = { {0, 99}, {0, 7}, { 0, 15 }, { 0, 3 }, { 0, 3 } };		//Selection index
int objectDropCategory = 0;				//0= gameplay (enemies and greenies), 1 = bad art 4x2, 2 = small 2x2, 3= tall 2x3, 4= appliance 3x2
int objectDropType = 0;					//index of object within its category
int objectPlacePalette = 0;
bool objectPlaceDir = false;
bool editDisableCursor = false;			//Mostly just for object select menu

int objectIndexNext = 0;				//Which object # drops next
int objectIndexLast = 0;				//The one we dropped last (for setting vars on a placed object for instance)

bool eraseFlag = false;

bool testAnimateState = false;

enum mapNames { null, hallway, condo, elevator, boss, jail };		//What map we're in
mapNames currentMap = null;
mapNames lastMap = null;				//Store currentMap here when Bud Dies, since Jail is also a location. After jail, goto lastMap

int highestObjectIndex = 0;				//Counts up as a level loads. When running logic/collision checks, don't search past this


int kittenMessageTimer = 0;

int budDeath = 0;						//If set, move Bud to center of screen then switch to KITTEN JAIL!!!

int jailYpos;
int budLives;
int budPower;
int powerMax = 3;
int budBlink;
int budPalette;							//Uber hack to make Bud blink when invincible, if blink & 1 budPalette = 8 which is blank palette RAM
int budStunned;
bool budStunnedDir;

int jailTimer = 0;			

bool fallingObjectState[16];			//Keep track of up to 16 falling objects (can't possible be more... right?)
int fallingObjectIndex[16];

int kittenMeow = 0;

int hallwayBudSpawn = 0;				//0 = through glass 1-6 in front of a cleared condo door 10 = in front of elevator (when stage change or die/respawn in hallway)

int budExitingWhat = 0;					//1 = condo door 2 = elevator

bool budVisible = false;

int frames = 0;							//Used to count seconds
int seconds;							//How long it took to beat a floor
int minutes;

bool levelComplete = false;				//After each condo exit check all apt stats, if all = clear open elevator and set this true. Allows exit to next floor

uint8_t xShake = 0;
uint8_t yShake = 0;

bool gameplayPaused = false;

long score = 0;
long levelScore = 0;				//What your score was upon entering the level (for saves)

int kittenCount = 0;				//How many player has recused per level
int kittenTotal = 0;				//Total per game (added on level win)
int kittenFloor = 0;				//Total per floor (added to on condo clear)		
int kittenToRescue = 0;				//How many to rescue per floor

int propDamage = 0;					//Per condo
int propDamageTotal = 0;			//Total per game (added on level win)
int propDamageFloor = 0;			//Total per floor (added to on condo clear)

int robotKill = 0;					//Per condo
int robotKillTotal = 0;				//Total per game (added on level win)
int robotKillFloor = 0;				//Total per floor (added to on condo clear)

bool bonusPause = false;
int bonusPhase = 0;
int bonusTimer = 0;
long levelBonus = 0;

int robotsToSpawn = 0;				//How many robots to spawn in the hallway (same as floor #)
int robotsSpaceTimer = 0;			//Time between robot spawns, in ticks

bool breakWindow = true;

bool cutscene = false;

int cutAnimate = 0;						//Used to animate stuff in cutscenes
int cutSubAnimate = 0;
int cutSubAnimate2 = 0;
int whichScene = 0;					//Which cutscene frame to show
int explosionCount = 0;

int saveSlots[3] = {0, 0, 0};		//0 = no save data, 1 = save data exists

bool loadFlag = false;

struct repeating_timer timer60Hz;           // Audio track timer
struct repeating_timer timerWFGenerator;    // Timer for the waveform generator

int gameLoopState = 0;

int mewTimer = 0;						//Bud mewing for no reason. Just like real life!

int difficulty = 2;						//1= Kitten 2 = Cat 3 = Dark Souls of Pumas
int robotSpeed = 1;						//Diff 3 speed = 2 else 1
int stunTimeStart = 90;					//70 = hard 90 normal 110 easy

bool levelCheat = false;

int kittyBonus = 0;
int robotBonus = 0;
int propBonus = 0;

bool waitPriorityChange = false;		//Used to fix one bug, I don't give a shit at this point

int bestTime[6][2] = { {3, 30}, {3, 40}, { 4, 15 }, { 4, 15 }, { 4, 20 }, {4, 30} };

int floorSeconds;				//Turn dumb minutes/seconds into seconds
int timeBonus;					//Pre-calc bonus for time (thanks CHAT-GPT, our new overlord masters)

//Master loops
void setup() { //------------------------Core0 handles the file system and game logic

	gamebadge3init();						//Init system

	menuTimer = 0;

	//add_repeating_timer_ms(-33, timer_isr, NULL, &timer30Hz);		//Start 30Hz interrupt timer. This drives the whole system

	add_repeating_timer_us(WAVE_TIMER, wavegen_callback, NULL, &timerWFGenerator);   
    add_repeating_timer_us(HERTZ60, sixtyHz_callback, NULL, &timer60Hz);						// NTSC audio timer

	music.AddTrack(intro, 0);		//Intro
	//Floors
	music.AddTrack(stage_1, 1);		//Castlevania 1 Vampire Killer	
	music.AddTrack(stage_2, 2);		//Mad gears
	music.AddTrack(stage_3, 3);		//Stream
	music.AddTrack(stage_4, 4);		//
	music.AddTrack(stage_5, 5);		//
	
	music.AddTrack(wiley, 6);

	music.AddTrack(titleMusic, 9);
	music.AddTrack(saveLoad, 10);
	music.AddTrack(game_over, 11);
	music.AddTrack(selection, 12);
	music.AddTrack(end_music, 13);
	
	music.AddTrack(ping, 15);
	
	//Serial.begin(115200);
	
}

void setup1() { //-----------------------Core 1 builds the tile display and sends it to the LCD



  
}

void loop() {	//-----------------------Core 0 handles the main logic loop
	
	if (nextFrameFlag > 1) {			//Counter flag from the 60Hz ISR, 2 ticks = 30 FPS
	
		nextFrameFlag -= 2;				//Don't clear it, just sub what we're "using" in case we missed a frame (maybe this isn't best with slowdown?)
		LCDsetDrawFlag();				//Tell core1 to start drawing a frame

		gameLoopState = 1;			

		serviceDebounce();				//Debounce buttons
	}

	if (displayPauseState == true) {  		//If paused for USB xfer, push START to unpause 
	
		if (button(start_but)) {        //Button pressed?
			displayPause(false);

			if (gameState == pauseMode) {			//USB menu pause? Go back to main menu (else game will resume I think?)		  
				switchGameTo(titleScreen);
			}
			else {
				music.PauseTrack(currentFloor);		//Probably needs to be current MUSIC?
			}
		}    
	}

	gameLoopLogic();					//Check this every loop frame
  
}

void loop1() { //------------------------Core 1 handles the graphics blitter. Core 0 can process non-graphics game logic while a screen is being drawn

	// if (nextFrameFlag == 2) {			//Counter flag from the 30Hz ISR
		// gpio_put(15, 1);
		// nextFrameFlag -= 2;				//Don't clear it, just sub what we're "using" in case we missed a frame (maybe this isn't best with slowdown?)
		// sendFrame();
		// gpio_put(15, 0);
	// }

	LCDlogic();



	// if (drawFrameFlag == true) {			//Flag set to draw display?
		// gpio_put(15, 1);
		// drawFrameFlag = false;				//Clear flag
		
		// // if (displayPause == true) {			//If display is paused, don't draw (return)
			// // return;
		// // }		

		// frameDrawing = true;				//Set flag that we're drawing a frame. Core0 should avoid accessing video memory if this flag is TRUE
		// sendFrame();						//Render the sprite-tile frame and send to LCD via DMA
		// frameDrawing = false;				//Clear flag. Core0 is now free to access screen memory (I guess this core could as well?)
		// gpio_put(15, 0);
	// }



}

void gameLoopLogic() {

	switch(gameLoopState) {
	
		case 0:
			//Do nothing (is set to 1 externally)
		break;
		
		case 1:		
			gpio_put(15, 1);
			serviceAudio();						//Service PCM audio file streaming while Core1 draws the screens (gives Core0 something to do while we wait for vid mem access)
			gameLoopState = 2;
			gpio_put(15, 0);
		break;		
		
		case 2:									//Done with our audio, now waiting for frame to finish rendering (we can start our vid mem logic during the final DMA)
			if (getRenderStatus() == false) {	//Wait for core 1 to finish drawing frame
				gameLoopState = 3;				//Jump to next check
			}
		break;
		
		case 3:									//Frame started?
			gpio_put(15, 1);
			gameFrame();	
			gameLoopState = 0;					//Done, wait for next frame flag
			gpio_put(15, 0);
		break;		
			
	}

}

void gameFrame() {

	switch(gameState) {

		case bootingMenu:							//Special type of load that handles files not being present yet
			if (menuTimer == 65534) {				//Let one frame be drawn to show the LOAD USB FILES message then...
				displayPause(true);				//Spend most of the time NOT drawing the screen so files can load easier
			}

			if (menuTimer > 0) {					//If timer active, decrement
				--menuTimer;				
			}
			
			if (menuTimer == 0) {					//First boot, or did timer reach 0?
				if (loadRGB("UI/NEStari.pal")) {				//Try and load master pallete from root dir (only need to load this once)
					//Load success? Load the rest
					loadPalette("UI/basePalette.dat");          //Load palette colors from a YY-CHR file. Can be individually changed later on
					loadPattern("UI/logofont.nes", 0, 256);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)					
					switchGameTo(splashScreen);					//Progress to splashscreen					
				}
				else {								//No files? Make USB file load prompt using flash text patterns and wait for user reset
					displayPause(false);   		//Just draw this one
					menuTimer = 65535;				//Try again in 36 minutes - or more likely, after user presses RESET after file xfer :)
					setWindow(0, 0);
				}							
			}

			break;
			
		case splashScreen:
			if (isDrawn == false) {
				drawSplashScreen();
			}	
			if (--menuTimer == 0) {					//Dec timer and goto main menu after timeout
				fillTiles(0, 0, 14, 14, 0, 3);		
				switchGameTo(titleScreen);
			}
			break;
				
		case titleScreen:
			if (isDrawn == false) {
				drawTitleScreen();
			}	
			
			menuTimer--;
			
			if (menuTimer == 20) {						//Bud blinks
				drawTile(2, 11, 8, 14, 4, 0);
				drawTile(3, 11, 9, 14, 4, 0);
				drawTile(2, 12, 8, 15, 4, 0);
				drawTile(3, 12, 9, 15, 4, 0);
				
				drawTile(5, 10, 10, 14, 4, 0);
				drawTile(6, 10, 11, 14, 4, 0);
				drawTile(5, 11, 10, 15, 4, 0);
				drawTile(6, 11, 11, 15, 4, 0);
				
			}
			
			if (menuTimer == 0) {						//Un-blink face in the lazier way possible
				menuTimer = random(50, 150);
				
				for (int y = 8 ; y < 16 ; y++) {				//Draw BUD FACE in palette 4
					for (int x = 0 ; x < 8 ; x++) {		
						drawTile(x, y - 1, x, y, 4, 0);
					}		
				}			
			}

			if (++cursorTimer > 3) {
				cursorTimer = 0;
				cursorAnimate += 0x10;
				if (cursorAnimate > 0x9F) {
					cursorAnimate = 0x8A;
				}
			}
			
			fillTiles(cursorX, 10, cursorX, 13, '@', 0);					//Erase arrows
			
			drawTile(cursorX, cursorY, cursorAnimate, 0, 0);			//Animated arrow

			if (levelCheat == true) {
				if (button(left_but)) {
					if (--currentFloor < 1) {
						currentFloor = 6;
					}
				}
				if (button(right_but)) {
					if (++currentFloor > 6) {
						currentFloor = 1;
					}
				}	

				tileDirect(12, 9, (((currentFloor - 1) * 16) + 128) + 15);				
			}

			if (button(up_but)) {
				if (cursorY > 10) {
					cursorY--;
					//music.PlayTrack(15);
					//pwm_set_freq_duty(6, 493, 25);
					soundTimer = 10;
				}				
			}
			if (button(down_but)) {
				if (cursorY < 13) {
					cursorY++;
					//music.PlayTrack(15);
					//pwm_set_freq_duty(6, 520, 25);
					soundTimer = 10;
				}	
				if (button(C_but)) {
					levelCheat = true;					
				}
				
			}
			
			if (button(A_but)) {
				menuTimer = 0;
				music.StopTrack(9);				//In case title music is playing
				switch(cursorY) {					
					case 10:		
						switchGameTo(diffSelect);		//New game, start by selecting difficulty					
					break;
					
					case 11:
						//switchGameTo(theEnd);
						switchGameTo(loadGame);
					break;
					
					case 12:
						editType = true;			//Press A to edit condos 1-6, 1-6
						switchGameTo(levelEdit);
					break;
					
					case 13:
						switchGameTo(pauseMode);				
					break;				
				}		
			}
			
			// if (button(B_but) && cursorY == 11) {		//DEV MODE - DISABLE
				// menuTimer = 0;				
				// editType = false;					//Press B to edit hallway
				// cutscene = true;					//Don't pre-draw doors or anything
				// switchGameTo(levelEdit);	
			// }

			//TIMER TO ATTRACT
			//SELECT START
		break;
		
		case diffSelect:
			if (isDrawn == false) {
				setupDiffSelect();
			}
			else {
				diffSelectLogic();
			}			
			break;

		case levelEdit:
			if (isDrawn == false) {
				setupEdit();
			}		
			if (editEntryFlag == true) {		//user must release A button before we can edit (to avoid false drops after removing debounce)
				if (button(A_but) == false) {
					editEntryFlag = false;
				}
			}
			else {
				editLogic();
			}			
		break;
					
		case game:	
			if (loadFlag == true) {
				loadFlag = false;
				resumeGame();
			}
			else {
				//currentFloor = 1;
				startGame();	
			}
			break;
			
		case story:
			if (isDrawn == false) {
				setupStory();
			}
			storyLogic();
			break;
			
		case goHallway:
			if (isDrawn == false) {
				setupHallway();
				currentMap = hallway;		//Hallway
			}
			hallwayLogic();
			break;
			
		case goCondo:
			if (isDrawn == false) {
				setupCondo(currentFloor, currentCondo);
				currentMap = condo;		//Condo
			}
			condoLogic();			
			break;
			
		case goElevator:
			if (isDrawn == false) {
				setupElevator();
				menuTimer = 100;
			}
			elevatorLogic();
			break;		
	
		case goJail:
			if (isDrawn == false) {
				lastMap = currentMap;			//Store where we were (where we should respawn)
				setupJail();
				currentMap = jail;
			}
			jailLogic();
			break;

		case gameOver:
			if (isDrawn == false) {
				setupGameOver();
			}
			gameOverLogic();
			break;

		case loadGame:
			if (isDrawn == false) {
				setupLoad();
			}
			loadLogic();
			break;	

		case saveGame:
			if (isDrawn == false) {
				setupSave();
			}
			saveLogic();
			break;	
			
		case pauseMode:
			if (isDrawn == false) {				//Draw menu...
				drawPauseMenu();
			}
			else {
				displayPause(true);			//and then pause on next frame
			}	
			break;	

		case theEnd:
			if (isDrawn == false) {
				setupEnding();
			}
			endingLogic();
			break;
						
		break;
		
		
	}

	if (displayPauseState == false) {  		//Screen NOT paused?
		if (button(start_but)) {        	//Button pressed?		
			if (gameState == goCondo || gameState == goHallway) {		//Only times you can PAWS get it?
				displayPause(true);
				playAudio("audio/pause_K.wav", 100);
				music.PauseTrack(currentFloor);		//Probably needs to be current MUSIC?								
			}
		}    
	}


	
}

void switchGameTo(stateMachineGame x) {		//Switches state of game

	displayPause(true);
	gameState = x;		
	isDrawn = false;
	
}

void displayPause(bool state) {

	displayPauseState = state;	
	pauseLCD(state);

}

void drawSplashScreen() {

	fillTiles(0, 0, 14, 14, 0, 3);
	
	for (int y = 0 ; y < 7 ; y++) {		
		for (int x = 0 ; x < 15 ; x++) {		
			drawTile(x, y, x, y, 0, 0);
		}		
	}
	
	for (int y = 0 ; y < 3 ; y++) {		
		for (int x = 0 ; x < 15 ; x++) {		
			drawTile(x, y + 8, x, y + 8, 1, 0);
		}		
	}	
	
	for (int y = 0 ; y < 2 ; y++) {		
		for (int x = 0 ; x < 15 ; x++) {		
			drawTile(x, y + 12, x, y + 11, 3, 0);
		}		
	}		

	setWindow(0, 0);
	menuTimer = 60;					//Splash Screen for 2 second
	//displayPause = false;   		//Allow core 2 to draw
	displayPause(false);
	isDrawn = true;
	
	playAudio("audio/gameBadge.wav", 100);		//Splash audio
	
}

void drawTitleScreen() {

	loadPalette("title/title_0.dat");                //Load palette colors from a YY-CHR file. Can be individually changed later on
	loadPattern("title/title_0.nes", 0, 256);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)					

	fillTiles(0, 0, 14, 14, '@', 3);					//Clear screen
	
	for (int y = 0 ; y < 6 ; y++) {				//Draw GAME TITLE
		for (int x = 0 ; x < 15 ; x++) {		
			drawTile(x, y, x, y, 1, 0);
		}		
	}	
	
	
	for (int y = 8 ; y < 16 ; y++) {				//Draw BUD FACE in palette 4
		for (int x = 0 ; x < 8 ; x++) {		
			drawTile(x, y - 1, x, y, 4, 0);
		}		
	}

	for (int g = 0 ; g < 5 ; g++) {
		
		tileDirect(10 + g, 8, 0xD8 + g);
	}


	drawText("start", 10, 10, false);
	drawText("load", 10, 11, false);	
	drawText("edit", 10, 12, false);		
	drawText("usb", 10, 13, false);	

	setWindow(0, 0);
	
	setButtonDebounce(up_but, true, 1);			//Set debounce on d-pad for menu select
	setButtonDebounce(down_but, true, 1);
	setButtonDebounce(left_but, true, 1);
	setButtonDebounce(right_but, true, 1);	
	
	setButtonDebounce(A_but, true, 1);
	setButtonDebounce(B_but, true, 1);
	setButtonDebounce(C_but, false, 0);
	
	menuTimer = random(50, 150);
	cursorTimer = 0;
	cursorX = 9;
	cursorY = 10;
	cursorAnimate = 0x8A;
					
	currentFloor = 1;			//This is where we start. We don't define it in startGame else the cheat in the menu wouldn't work. BUT we have to reset it here else game will start on last active level #				
					
	isDrawn = true;	
	displayPause(false);
	
	music.PlayTrack(9, false);
	
}

void drawPauseMenu() {
	
	loadPattern("condo/condo.nes", 0, 256);			//Table 0 = condo tiles
	
	fillTiles(0, 0, 14, 14, ' ', 3);			//CLS
		    //0123456789ABCDE
	drawText("LCD DMA PAUSED", 0, 0, false);
	drawText(" TRANSFER USB", 0, 6, false);
	drawText(" THUMB DRIVE", 0, 7, false);
	drawText("  FILES NOW", 0, 8, false);
	
	drawText("START=UNPAUSE", 0, 14, false);
	
	setWindow(0, 0);	
	
	//Serial.println("LCD DMA PAUSED");
	
	isDrawn = true;
	displayPause(false);   		//Allow core 2 to draw
	
}


//Gameplay-----------------------------------------------

void setupLoad() {
	
	loadPalette("ui/saveLoad.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
	loadPattern("ui/saveLoad.nes", 0, 256);			//Table 0 = condo tiles		
	
	fillTiles(0, 0, 31, 15, ' ', 0);			//Black BG
			
	setWindow(0, 0);
			//0123456789ABCDE
	drawText("     SELECT", 0, 4, false);			
	drawText("   LOAD SLOT", 0, 5, false);

	drawText("SLOT 1", 6, 7, false);
	drawText("SLOT 2", 6, 8, false);
	drawText("SLOT 3", 6, 9, false);

	checkSaveSlots();

	for (int x = 0 ; x < 3 ; x++) {
		if (saveSlots[x] == 1) {
			drawTile(4, x + 7, 0x1E, 4, 0);		//Face
		}	
		else {
			drawTile(4, x + 7, 0x1F, 4, 0);		//Empty square
		}
	}

	drawText("A = LOAD", 3, 12, false);
	drawText("B = MENU", 3, 13, false);	
	drawText("C = ERASE", 3, 14, false);
	
	cursorY = 7;
	cursorAnimate = 0x0C;
	
	setButtonDebounce(up_but, true, 1);
	setButtonDebounce(down_but, true, 1);

	displayPause(false);   		//Allow core 2 to draw
	isDrawn = true;	
		
	music.PlayTrack(10, true);	
	
}

void loadLogic() {
	
	fillTiles(2, 7, 2, 9, ' ', 0);						//Erase arrows
	
	drawTile(2, cursorY, cursorAnimate, 0, 0);			//Animated arrow

	if (++cursorTimer > 3) {
		cursorTimer = 0;
		cursorAnimate += 0x10;
		if (cursorAnimate > 0x1C) {
			cursorAnimate = 0x0C;
		}
	}

	if (button(up_but)) {
		if (cursorY > 7) {
			cursorY--;
			//music.PlayTrack(15);
			//pwm_set_freq_duty(6, 493, 25);
			soundTimer = 10;
		}				
	}
	if (button(down_but)) {
		if (cursorY < 9) {
			cursorY++;
			//music.PlayTrack(15);
			//pwm_set_freq_duty(6, 520, 25);
			soundTimer = 10;
		}				
	}
	
	if (button(A_but)) {
		if (saveSlots[cursorY - 7] == 1) {
			music.StopTrack(10);
			menuTimer = 0;
			loadGameFile(cursorY - 7);	
			loadFlag = true;			//When switch to game mode, this resumes instead of starts new
			switchGameTo(game);			
			
		}
	}	

	if (button(B_but)) {
		music.StopTrack(10);
		switchGameTo(titleScreen);
	}
	
	if (button(C_but)) {	
		if (saveSlots[cursorY - 7] == 1) {
			eraseSaveSlots(cursorY - 7);
			switchGameTo(loadGame);
		}
	}

	if (saveSlots[cursorY - 7] == 0) {		
		drawText("    NO DATA", 0, 11, false);	
	}
	else {
		drawText("           ", 0, 11, false);
	}

		
}

void setupSave() {
	
	loadPalette("ui/saveLoad.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
	loadPattern("ui/saveLoad.nes", 0, 256);			//Table 0 = condo tiles		
	
	fillTiles(0, 0, 31, 15, ' ', 0);			//Black BG
			
	setWindow(0, 0);
			//0123456789ABCDE
	drawText("     SELECT", 0, 4, false);			
	drawText("   SAVE SLOT", 0, 5, false);

	drawText("SLOT 1", 6, 7, false);
	drawText("SLOT 2", 6, 8, false);
	drawText("SLOT 3", 6, 9, false);

	drawText("  LEVEL SCORE", 0, 13, false);
	drawDecimal(levelScore, 14);		//Draw score + bonus but don't actually add it til the end

	checkSaveSlots();

	for (int x = 0 ; x < 3 ; x++) {
		if (saveSlots[x] == 1) {
			drawTile(4, x + 7, 0x1E, 4, 0);		//Face
		}	
		else {
			drawTile(4, x + 7, 0x1F, 4, 0);		//Empty square
		}
	}

	cursorY = 7;
	cursorAnimate = 0x0C;
	
	setButtonDebounce(up_but, true, 1);
	setButtonDebounce(down_but, true, 1);

	displayPause(false);   		//Allow core 2 to draw
	isDrawn = true;	
		
	music.PlayTrack(10, true);
	
}

void saveLogic() {
	
	fillTiles(2, 7, 2, 9, ' ', 0);						//Erase arrows
	
	drawTile(2, cursorY, cursorAnimate, 0, 0);			//Animated arrow

	if (++cursorTimer > 3) {
		cursorTimer = 0;
		cursorAnimate += 0x10;
		if (cursorAnimate > 0x1C) {
			cursorAnimate = 0x0C;
		}
	}

	if (button(up_but)) {
		if (cursorY > 7) {
			cursorY--;
			//music.PlayTrack(15);
			//pwm_set_freq_duty(6, 493, 25);
			soundTimer = 10;
		}				
	}
	if (button(down_but)) {
		if (cursorY < 9) {
			cursorY++;
			//music.PlayTrack(15);
			//pwm_set_freq_duty(6, 520, 25);
			soundTimer = 10;
		}				
	}
	
	if (button(A_but)) {
		music.StopTrack(10);
		menuTimer = 0;
		saveGameFile(cursorY - 7);	
		switchGameTo(titleScreen);
	}	

	if (saveSlots[cursorY - 7] == 1) {		
		drawText("A=OVERWRITE", 2, 11, false);	
	}
	else {
		drawText("            ", 0, 11, false);
	}
	
	
}

void checkSaveSlots() {

	if (loadFile("/saves/slot_1.sav")) {
		saveSlots[0] = 1;
	}
	else {
		saveSlots[0] = 0;
	}
	closeFile();
	if (loadFile("/saves/slot_2.sav")) {
		saveSlots[1] = 1;
	}
	else {
		saveSlots[1] = 0;
	}
	closeFile();	
	if (loadFile("/saves/slot_3.sav")) {
		saveSlots[2] = 1;
	}
	else {
		saveSlots[2] = 0;
	}
	closeFile();	
	
}

void eraseSaveSlots(int which) {

	switch(which) {
		
		case 0:
			eraseFile("/saves/slot_1.sav");
		break;
		case 1:
			eraseFile("/saves/slot_2.sav");
		break;		
		case 2:
			eraseFile("/saves/slot_3.sav");
		break;

	}
	
}


void saveGameFile(int slot) {

	switch(slot) {		
		case 0:
			saveFile("saves/slot_1.sav");
		break;
		case 1:
			saveFile("saves/slot_2.sav");
		break;		
		case 2:
			saveFile("saves/slot_3.sav");
		break;	
	}

	writeByte(currentFloor);
	writeByte(difficulty);
	writeByte(levelScore >> 24);			//Save the last level score (what your score was when level started)
	writeByte((levelScore >> 16) & 0xFF);
	writeByte((levelScore >> 8) & 0xFF);
	writeByte(levelScore & 0xFF);

	writeByte((kittenTotal >> 8) & 0xFF);	//Same, save your last progress. There's no point in saving on level 1
	writeByte(kittenTotal & 0xFF);
	writeByte((robotKillTotal >> 8) & 0xFF);
	writeByte(robotKillTotal & 0xFF);
	writeByte((propDamageTotal >> 8) & 0xFF);
	writeByte(propDamageTotal & 0xFF);	

	// for (int x = 0 ; x < 6 ; x++) {		//Clear next set of condos
		// writeByte(apartmentState[x]);
	// }
	
	closeFile();
	
}

void loadGameFile(int slot) {

	switch(slot) {		
		case 0:
			loadFile("saves/slot_1.sav");
		break;
		case 1:
			loadFile("saves/slot_2.sav");
		break;		
		case 2:
			loadFile("saves/slot_3.sav");
		break;	
	}
	
	currentFloor = readByte();
	difficulty = readByte();
	
	powerMax = 5 - difficulty;
	if (difficulty == 3) {
		robotSpeed = 2;
	}
	else {
		robotSpeed = 1;
	}
	
	stunTimeStart = (130 - (difficulty * 20));		//70 = hard 90 normal 110 easy
	
	//Load the last level score
	
	levelScore = 0;	
	levelScore = (readByte() << 24) | (readByte() << 16) | (readByte() << 8) | readByte();

	score = levelScore;		//Score = level score

	kittenTotal = 0;
	kittenTotal = (readByte() << 8) | readByte();
	
	robotKillTotal = 0;
	robotKillTotal = (readByte() << 8) | readByte();
	
	propDamageTotal = 0;
	propDamageTotal = (readByte() << 8) | readByte();	


	for (int x = 0 ; x < 6 ; x++) {		//Clear next set of condos
		apartmentState[x] = 0;
	}
	
	closeFile();
	
}


void setupDiffSelect() {

	loadPalette("ui/saveLoad.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
	loadPattern("ui/saveLoad.nes", 0, 256);			//Table 0 = condo tiles
	loadPattern("title/bud_face.nes", 256, 256);	//Table 1 = Bud Face sprites

	fillTiles(0, 0, 14, 14, ' ', 0);			//Black BG
			
	setWindow(0, 0);
	
			//0123456789ABCDE				//Draw score centered
	drawText("    SELECT", 0, 0, false);		
	drawText("  DIFFICULTY", 0, 1, false);

	cursorTimer = 0;
	cursorAnimate = 0x0C;

	setButtonDebounce(left_but, true, 1);
	setButtonDebounce(right_but, true, 1);

	displayPause(false);   		//Allow core 2 to draw
	isDrawn = true;	
	
	budDir = false;
	menuTimer = 0;
	budY = 0;
	
	music.PlayTrack(12, true);
	
}

void diffSelectLogic() {

	fillTiles(0, 13, 14, 14, ' ', 0);					//Erase text

	if (++cursorTimer > 3) {
		cursorTimer = 0;
		cursorAnimate += 0x10;
		if (cursorAnimate > 0x1C) {
			cursorAnimate = 0x0C;
		}
	}

	drawTile(14, 1, cursorAnimate, 0, 0);			//Animated arrow right
	drawTile(0, 1, cursorAnimate + 1, 0, 0);			//Animated arrow left

	switch(difficulty) {
		
		case 1:
			drawSprite(28, 28 + budY, 0, 16, 8, 8, 4, false, false);
			drawText("     KITTEN", 0, 13, false);
		break;
		
		case 2:
			drawSprite(28, 28 + budY, 8, 16, 8, 8, 4, false, false);
			drawText("      CAT", 0, 13, false);
		break;
			        //0123456789ABCDE				//Draw score centered			
		case 3:
			drawSprite(28, 28 + budY, 0, 16 + 8, 8, 8, 4, false, false);
			drawText("THE DARK SOULS", 0, 13, false);
			drawText("   OF PUMAS", 0, 14, false);
		break;
		
	}

	if (++menuTimer == 5) {
		menuTimer = 0;
		if (budDir == false) {
			if (++budY == 3) {
				budDir = true;
			}
		}
		else {
			if (--budY == 0) {
				budDir = false;
			}		
		}	
	}

	bool changed = false;

	if (button(left_but)) {
		if (--difficulty < 1) {
			difficulty = 3;
		}
		changed = true;		
	}
	if (button(right_but)) {
		if (++difficulty > 3) {
			difficulty = 1;
		}	
		changed = true;		
	}
	
	if (changed) {
	
		switch(difficulty) {
			case 1:
				playAudio("audio/kitmeow3.wav", 90);
			break;
			
			case 2:
				playAudio("audio/bud_mew.wav", 90);
			break;
	
			case 3:
				playAudio("audio/lion.wav", 90);
			break;
		}
	}
		
	if (button(A_but)) {
		music.StopTrack(12);		
		startNewGame();				
	}

}

void startNewGame() {
	
	powerMax = 5 - difficulty;
	
	if (difficulty == 3) {
		robotSpeed = 2;
	}
	else {
		robotSpeed = 1;
	}
	
	levelComplete = false;
	
	stunTimeStart = (130 - (difficulty * 20));		//70 = hard 90 normal 110 easy
	
	loadFlag = false;				//NEW GAME				
	whichScene = 0;
	music.PlayTrack(0, false);
	switchGameTo(story);		
	
}

void setupGameOver() {

	loadPalette("ui/saveLoad.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
	loadPattern("ui/saveLoad.nes", 0, 256);			//Table 0 = condo tiles		

	fillTiles(0, 0, 31, 15, ' ', 0);			//Black BG
			
	setWindow(0, 0);
	
			//0123456789ABCDE
	drawText("  TOTAL SCORE", 0, 13, false);
	drawDecimal(score, 14);		
	
	drawText("   GAME OVER", 0, 2, false);

	drawText("  CONTINUE", 0, 6, false);
	drawText("  SAVE PROGRESS", 0, 7, false);
	drawText("  QUIT TO MENU", 0, 8, false);

	cursorY = 6;
	cursorTimer = 0;
	cursorAnimate = 0x0C;

	setButtonDebounce(up_but, true, 1);
	setButtonDebounce(down_but, true, 1);

	displayPause(false);   		//Allow core 2 to draw
	isDrawn = true;	
	
	music.PlayTrack(11, false);
	
}

void gameOverLogic() {

	fillTiles(1, 6, 1, 8, ' ', 0);					//Erase arrows
	
	drawTile(1, cursorY, cursorAnimate, 0, 0);			//Animated arrow

	if (++cursorTimer > 3) {
		cursorTimer = 0;
		cursorAnimate += 0x10;
		if (cursorAnimate > 0x1C) {
			cursorAnimate = 0x0C;
		}
	}

	if (button(up_but)) {
		if (cursorY > 6) {
			cursorY--;
			//music.PlayTrack(15);
			//pwm_set_freq_duty(6, 493, 25);
			soundTimer = 10;
		}				
	}
	if (button(down_but)) {
		if (cursorY < 8) {
			cursorY++;
			//music.PlayTrack(15);
			//pwm_set_freq_duty(6, 520, 25);
			soundTimer = 10;
		}				
	}
	
	if (button(A_but)) {
		
		music.StopTrack(11);
		
		menuTimer = 0;
		switch(cursorY) {					
			case 6:		
				continueLevel();				
			break;
			
			case 7:
				switchGameTo(saveGame);
			break;

			case 8:
				switchGameTo(titleScreen);				
			break;				
		}		
	}


	
}

void continueLevel() {
	
	powerMax = 5 - difficulty;
	
	score = levelScore;		//Reset score to whatever it was when we entered level
	
	propDamage = 0;			//Reset counts (but not totals)
	propDamageFloor = 0;
	
	robotKill = 0;
	robotKillFloor = 0;
	
	kittenCount = 0;
	kittenFloor = 0;

	budLives = 3;
	budPower = powerMax;
	budStunned = 0;
	
	stopWatchClear();
		
	for (int x = 0 ; x < 6 ; x++) {		//Clear next set of condos
		apartmentState[x] = 0;
	}	
	levelComplete = false;
	
	currentCondo = 0;			//Bud hasn't entered a condo
	
	hallwayBudSpawn = 11;		//Continue spawns in front of closed elevator
	switchGameTo(goHallway);	//Spawn
	
}

void startGame() {

	score = 0;
	levelScore = 0;

	propDamage = 0;
	propDamageFloor = 0;
	propDamageTotal = 0;	
	robotKill = 0;
	robotKillFloor = 0;
	robotKillTotal = 0;	
	kittenCount = 0;
	kittenFloor = 0;
	kittenTotal = 0;

	//currentFloor = 1;			//Setup game here
	currentCondo = 0;			//Bud hasn't entered a condo
	budLives = 3;
	budPower = powerMax;
	hallwayBudSpawn = 0;		//Bud jumps through glass

	jump = 0;
	velocikitten = 0;

	budStunned = 0;

	elevatorOpen = false;		//Level starting (if die in hallway, spawn in front of closed elevator)
	
	stopWatchClear();
	
	for (int x = 0 ; x < 6 ; x++) {		//Clear next set of condos
		apartmentState[x] = 0;
	}	
	levelComplete = false;
	
	breakWindow = true;
	
	switchGameTo(goHallway);

}

void resumeGame() {
	
	propDamage = 0;
	propDamageFloor = 0;
	robotKill = 0;
	robotKillFloor = 0;
	kittenCount = 0;
	kittenFloor = 0;
	
	score = levelScore;			//Your loaded score is whatever score you had upon entering the level where you died and saved

	currentCondo = 0;			//Bud hasn't entered a condo
	budLives = 3;
	budPower = powerMax;
	hallwayBudSpawn = 11;		//For a load, spawn in front of elevator (only crash window for new game start)

	jump = 0;
	velocikitten = 0;

	budStunned = 0;

	elevatorOpen = false;		//Level starting (if die in hallway, spawn in front of closed elevator)
	
	for (int x = 0 ; x < 6 ; x++) {		//Clear next set of condos
		apartmentState[x] = 0;
	}	
	levelComplete = false;
	
	stopWatchClear();
		
	breakWindow = false;
	
	switchGameTo(goHallway);	
	
}


void startNewFloor() {

	currentFloor++;
	currentCondo = 0;			//Bud hasn't entered a condo on this floor yet

	budExitingWhat =  2;				//Elevator
	hallwayBudSpawn = 10;				//Bud spawns in front of the elevator
	elevatorOpen = false;				//Level starting (if die in hallway, spawn in front of closed elevator)
	
	budVisible = false;
	budState = exiting;
	budFrame = 0;
	jump = 0;	
	
	stopWatchClear();
	switchGameTo(goHallway);	//

	
}

void setupJail() {

	music.StopTrack(currentFloor);

	fillTiles(0, 0, 31, 15, ' ', 0);			//Black BG
			
	setWindow(0, 0);
	menuTimer = 60;				//Jail for 4 seconds

	jailYpos = -80;

	displayPause(false);   		//Allow core 2 to draw
	isDrawn = true;		
	
}

void jailLogic() {

	drawBudStats();

	drawSprite(40, jailYpos, 8, 16 + 10, 5, 5, 7, false, false);

	drawSprite(40, 80, 0x01F8, 0, false, false);
	drawSprite(56, 80, 0x01F9, 0, false, false);
	drawSpriteDecimal(budLives, 72, 80, 0);

	drawText("  TOTAL SCORE", 0, 13, false);
	drawDecimal(score + levelBonus, 14);		//Draw score + bonus but don't actually add it til the end
	
	if (menuTimer > 50) {
		drawSprite(52, 47, 13, 16 + 13, 2, 3, 4, false, false);		//Sitting
	}
	else {
		drawSprite(52, 47, 13, 16 + 10, 2, 3, 4, false, false);		//Paws on bars
	}

	if (jailYpos == 24) {
		playAudio("audio/jaildoor.wav", 100);
		budLives--;
	}

	if (jailYpos < 32) {
		jailYpos += 8;
	}
	else {
		if (--menuTimer == 0) {
			
			if (budLives == 0) {
				switchGameTo(gameOver);			
			}
			else {
				budPower = powerMax;
				jump = 0;
				velocikitten = 0;
				switch(lastMap) {
					
					case condo:
						switchGameTo(goCondo);
					break;
					
					case hallway:
						budPower = powerMax;
						budStunned = 0;
						hallwayBudSpawn = 11;		//If die in hallway, spawns in front of closed elevator
						switchGameTo(goHallway);						
					break;				
				}			
			}
			
		}				
	}

}


//Condo---------------------------------------------------

void setupCondo(int whichFloor, int whichCondo) {
	
	mapWidth = condoWidth;
	
	stopAudio();
	
	clearObjects();

	loadPalette("condo/condo.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
	loadPattern("condo/condo.nes", 0, 256);			//Table 0 = condo tiles
	loadPattern("sprites/bud.nes", 256, 256);		//Load bud sprite tiles into page 1	
	loadPattern("sprites/objects.nes", 512, 256);	//Table 2 = condo objects
	loadPattern("sprites/robots.nes", 768, 256);	//Table 3 = robots

	setButtonDebounce(up_but, true, 1);		//Debounce UP for door entry
	setButtonDebounce(down_but, false, 0);
	setButtonDebounce(left_but, false, 0);
	setButtonDebounce(right_but, false, 0);
	
	setButtonDebounce(A_but, true, 1);
	setButtonDebounce(B_but, true, 1);
	setButtonDebounce(C_but, true, 1);
	
	setCoarseYRollover(0, 14);   //Sets the vertical range of the tilemap, rolls back to 0 after 29

	setWindow(0, 0);

	fileNameFloor = whichFloor + 48;		//Convert the numbers to ASCII
	fileNameCondo = whichCondo + 48;
	updateMapFileName();
	loadLevel();
	

	int windowStartCoarseX = 0;

	int budStartCoarseX = 1;
	
	budState = rest;
	
	budFrame = 0;
	budSubFrame = 0;
	budDir = false;											//True = going left

	budX = budStartCoarseX * 8;								//Position of Bud on LCD, LCD coord
	budY = 104; //budStartFineY;
	
	budWorldX = 8 * (windowStartCoarseX + budStartCoarseX);			//Bud's position in the world (not the same as screen or tilemap)
	budWorldY = budY; //budStartFineY;
	xWindowBudFine = 0;										//Used to track when Bud crosses tile barriers

	budSpawned = true;

	worldX = 8 * windowStartCoarseX;					//Where (fine) the camera is positioned in the world (not the same as tilemap as that's dynamic)
	worldY = 0;

	xWindowFine = 0;										//The fine scrolling amount (fine, across tilemap)
	xWindowCoarse = windowStartCoarseX & 0x1F;				//In the tilemap, the left edge of the current positiom (coarse)
	
	xStripLeft = xWindowCoarse - 5;	//6				//Set the left draw edge in the tilemap
	
	if (xStripLeft < 0) {							//Handle negative rollover
		xStripLeft += 32;
	}
	
	xStripRight = ((xStripLeft + 26) & 0x1F);		//Compute right edge and mask for rollover
	
	xLevelStripLeft = windowStartCoarseX - 5;		//Where to fetch left edge map strips (when moving left)
	
	if (xLevelStripLeft < 0) {						//Rollover left
		xLevelStripLeft += 120;
	}
	
	xLevelCoarse = windowStartCoarseX;				//In the map, the left edge of the current positiom (coarse)	
	xLevelStripRight = xLevelStripLeft + 26;		//Where to fetch right edge map strips (when moving right)
	xWindowBudToTile = xWindowCoarse + (budStartCoarseX + 1);		//Where Bud is in relation to the onscreen portion of the name table (not the level map) to detect platforms etc
		
	if (xLevelStripRight > 119) {					//Right edge rollover
		xLevelStripRight -= 120;
	}	
	

	redrawMapTiles();


	for (int x = 0 ; x < maxThings ; x++) {			//Find first open slot
		if (object[x].active == true) { // && object[x].category == 0) {		//If object exists and is a robot, turn movement on/off
			object[x].moving = true;
		}
	}

	propDamage = 0;		//Reset per condo stats
	robotKill = 0;
	kittenCount = 0;		//Reset rescue per condo
	kittenToRescue = 0;		//Reset total found

	for (int x = 0 ; x < maxThings ; x++) {			//Find first open slot
		if (object[x].active == true && object[x].category == 0 && object[x].type == 4) {		//Count kittens
			kittenToRescue++;
		}
	}

	isDrawn = true;	
	displayPause(false);
	
	kittenMessageTimer = 90;

	budStunned = 0;
	
	movingObjects(true);			//Tell robots to move

	if (music.IsTrackPlaying(currentFloor) == false) {
		music.PlayTrack(currentFloor, true);
	}
	
}

void redrawMapTiles() {							//Redraws all strips into tile map memory

	//0-119 condo map width

	int tileColumn = xStripLeft;				//Get starting left edge of tile	
	int mapColumn = xLevelStripLeft;			//and map

	for (int x = 0; x < 27 ; x++) {				//Draw 26 columns
		drawCondoStrip(mapColumn, tileColumn);	//Draw vertical strip
		++tileColumn &= 0x1F;					//Inc with binary rollover (32 wide)
		
		if (++mapColumn == mapWidth) {				//Rollover (static as it's always a single inc no remainder)
			mapColumn = 0;
		}
	}	
	
	// for (int y = 0 ; y < 15 ; y++) {							//Draw by row		
		// for (int xx = 0 ; xx < 15 ; xx++) {						//Draw across
			// tileDirect(xx, y, condoMap[y][xx]);
		// }
	// }	
	
}

void drawCondoStrip(int condoStripX, int nameTableX) {
	
	for (int y = 0 ; y < 15 ; y++) {							//Draw by row		
		tileDirect(nameTableX, y, condoMap[y][condoStripX]);
	}	
	
}	

void exitCondo() {
	
	stopAudio();
	
	apartmentState[currentCondo - 1] = 0xC0;	//Set apartment as door open + cleared
	
	hallwayBudSpawn = currentCondo;				//Bud spawns in front of the door
	
	budVisible = true;
	budExitingWhat = 1;			//Exiting condo
	budState = exiting;
	budFrame = 0;
	jump = 0;
	
	kittenMessageTimer = 0;				//Clear timer in case a message was on when Bud left
	
	kittenFloor += kittenCount;			//Condo cleared? Add how many kittens we recused to floor total
	robotKillFloor += robotKill;		//along with robots and property damage
	propDamageFloor += propDamage;
	
	int count = 0;
	
	for (int x = 0 ; x < 6 ; x++) {
		if (apartmentState[x] & 0x80) {
			count++;
		}
	}
	
	if (count == 6) {
		levelComplete = true;
		elevatorOpen = true;
		kittenMessageTimer = 60;		//In hallway logic, use this var to print GOTO THE ELEVATOR
	}
	
	//Add option so message is either "GOTO ELEVATOR" or "X CONDOS LEFT"

	switchGameTo(goHallway);
	
}

void condoLogic() {

	if (displayPauseState == true) {			//Pause? Do no logic
		return;
	}

	stopWatchTick();	

	//drawSpriteDecimal(minutes, 80, 0, 4);
	//drawSpriteDecimal(seconds, 100, 0, 6);

	
	// drawSpriteDecimal(budWorldX, 80, 8, 0);
	// drawSpriteDecimal(worldX, 80, 16, 0);
	// drawSpriteDecimal(propDamageTotal, 80, 24, 0);

	//drawSpriteDecimal(budX, 40, 8, 0);
	// drawSpriteDecimal(robotKillFloor, 40, 16, 0);
	// drawSpriteDecimal(propDamageFloor, 40, 24, 0);




	if (budState != dead) {						//Messages top sprite pri, but don't draw if Bud dying
		if (kittenMessageTimer > 0) {
				
			//0123456789ABCDE
			//   RESCUE XX
			//	  KITTENS
			// GOTO THE EXIT
			
			if (kittenMessageTimer & 0x04) {
				int offset = 4;						
				
				if ((kittenToRescue - kittenCount) > 9) {
					offset = 0;
				}
		
				if (kittenCount == kittenToRescue) {
					drawSpriteText("GOTO THE EXIT", 8, 56, 3);
				}
				else {
					drawSpriteText("RESCUE", 28 + offset, 52, 3);
					drawSpriteDecimal(kittenToRescue - kittenCount, 84 + offset, 52, 3);		//Delta of kittens to save
					
					if (kittenCount == (kittenToRescue - 1)) {		//Only one KITTENS left? Draw a sloppy slash over the S
						drawSpriteText("KITTEN", 36, 60, 3);					
					}	
					else {
						drawSpriteText("KITTENS", 36, 60, 3);						
					}
													
				}
			}	
			kittenMessageTimer--;
		}

		if (kittenCount == kittenToRescue) {
			
			drawSprite(7 - inArrowFrame, 88, 6, 16 + 12, 1, 2, 4, false, false);		//out arrow sprite <		
			drawSprite(16, 84, 7, 16 + 11, 1, 3, 4, false, false);		//OUT text
			
			if (++inArrowFrame > 7) {
				inArrowFrame = 0;
			}		
			
		}
	}

	//drawSpriteDecimalRight(score, 107, 4, 0);		//Right justified score
	drawBudStats();
	budLogic2();

	if (budState == dead) {
		if (menuTimer < 21) {
			return;
		}				
		if (menuTimer == 21) {
			fillTiles(0, 0, 31, 31, ' ', 0);			//Black BG				
			setWindow(0, 0);
			return;
		}
	}
	
	//drawSpriteDecimal(budStunned, 60, 0, 0);
	// drawSpriteDecimal(xLevelStripLeft, 8, 8, 0);
	// drawSpriteDecimal(xWindowCoarse, 8, 16, 0);
	// drawSpriteDecimal(xLevelStripRight, 8, 24, 0);	
	
	// drawSpriteDecimal(budX, 60, 0, 0);
	// drawSpriteDecimal(budWorldX, 90, 0, 0);	
	
	// drawSpriteDecimal(xWindowBudToTile, 80, 16, 0);
	// drawSpriteDecimal(budY >> 3, 80, 24, 0);
	// drawSpriteDecimal(jump, 80, 32, 0);

	//Draw lives and power bar

	setWindow((xWindowCoarse << 3) | xWindowFine + xShake, yPos + yShake);			//Set scroll window

	objectLogic();

}


//Hallway-------------------------------------

void setupHallway() {

	mapWidth = hallwayWidth;

	stopAudio();
	
	clearObjects();

	loadPalette("hallway/hallway.dat");            	//Load palette colors from a YY-CHR file. Can be individually changed later on
	loadPattern("hallway/hallway.nes", 0, 256);		//Load hallway tiles into page 0
	
	loadPattern("sprites/bud.nes", 256, 256);		//Load bud sprite tiles into page 1	
	loadPattern("sprites/objects.nes", 512, 256);	//Table 2 = condo objects
	loadPattern("sprites/robots.nes", 768, 256);	//Table 3 = robots	
	
	setButtonDebounce(up_but, true, 1);		//Debounce UP for door entry
	setButtonDebounce(down_but, false, 0);
	setButtonDebounce(left_but, false, 0);
	setButtonDebounce(right_but, false, 0);
	
	setButtonDebounce(A_but, true, 1);
	setButtonDebounce(B_but, true, 1);
	setButtonDebounce(C_but, true, 1);	
	
	setCoarseYRollover(0, 14);   //Sets the vertical range of the tilemap, rolls back to 0 after 29

	budDir = false;		//Always spawns facing right

	switch(hallwayBudSpawn) {
	
		case 0:				//Breaking glass stage 1
			budSpawned = false;
			budSpawnTimer = 0;
			budState = rest;
			spawnIntoHallway(0, 5, 40);
			break;
					
					
		case 11:				//Spawn in front of elevator (if died in hallway)
			budSpawned = true;	
			budFrame = 0;
			budVisible = false;
			budState = rest;
			jump = 0;
			spawnIntoHallway(60, 7, 104);
			break;					
					
		case 10:			//Emerge from elevator (stage 2-6)
			budSpawned = true;	
			elevatorOpen = false;
			budState = exiting;
			budExitingWhat = 2;			//exiting elevator
			budFrame = 0;
			budVisible = false;
			jump = 0;
			spawnIntoHallway(60, 7, 104);
			break;
			
		default:			//Emerge from cleared condo
			budSpawned = true;
			budDir = false;		
			spawnIntoHallway(doorTilePos[currentCondo - 1] - 6, 7, 104);		
			break;
	
	}
	
	//levelComplete = false;

	fileNameFloor = '1';		//Hallway hub is floor 1, condo 0
	fileNameCondo = '0';
	updateMapFileName();
	loadLevel();								//Load hallway level tiles (includes Greenies) we will populate objects and redraw doors manually

	placeObjectInMap(152, 0, 4, 32 + 8, 4, 3, 7, true, 1, 50);		//Chandeliers
	placeObjectInMap(296, 0, 4, 32 + 8, 4, 3, 7, true, 1, 50);
	placeObjectInMap(440, 0, 4, 32 + 8, 4, 3, 7, true, 1, 50);
	placeObjectInMap(616, 0, 4, 32 + 8, 4, 3, 7, true, 1, 50);	
	placeObjectInMap(760, 0, 4, 32 + 8, 4, 3, 7, true, 1, 50);	
	placeObjectInMap(904, 0, 4, 32 + 8, 4, 3, 7, true, 1, 50);	
				
	placeObjectInMap(32, 48, 4, 32 + 12, 4, 3, 6, true, 10, 5, 21);		//Spawn an intact window on object #21
	
	if (currentFloor == 1) {						//Broken window only on first floor
		if (currentCondo > 0) {						//Bud has already been in a condo? (not game opening)
			if (currentFloor == 1) {
				object[21].sheetX = 0;				//Window was already broken, change the above to broken
			}
		}				
	}
	//Else leave window intact, as it was spawned
	
	placeObjectInMap(1025, 48, 4, 32 + 12, 4, 3, 6, true, 10, 5, 23);		//Other window spawns as object #23
	
	populateEditDoors();			//Redraw stuff that changes (door #'s, door open close, etc)
	drawHallwayElevator(64);		

	movingObjects(true);			//Tell all objects they can move

	redrawMapTiles();					//Reload what's visible

	robotsToSpawn = currentFloor + 1; // * 2;	//How many robots spawn in the hallway (same as floor # + 1)
	robotsSpaceTimer = 90;			//Starting time between robot spawns, in ticks. Randomly changes after each spawn

	displayPause(false);   		//Allow core 2 to draw
	isDrawn = true;	
	
	if (music.IsTrackPlaying(currentFloor) == false) {
		music.PlayTrack(currentFloor, true);
	}
	
	waitPriorityChange = false;  //Hacky fix
	
}

void spawnIntoHallway(int windowStartCoarseX, int budStartCoarseX, int budStartFineY) {

	budFrame = 0;
	budSubFrame = 0;
	budDir = false;											//True = going left

	budX = budStartCoarseX * 8;								//Position of Bud on LCD, LCD coord
	budY = budStartFineY;
	
	budWorldX = 8 * (windowStartCoarseX + budStartCoarseX);			//Bud's position in the world (not the same as screen or tilemap)
	budWorldY = budStartFineY;
	xWindowBudFine = 0;										//Used to track when Bud crosses tile barriers

	worldX = 8 * windowStartCoarseX;					//Where the camera is positioned in the world (not the same as tilemap as that's dynamic)
	worldY = 0;

	xWindowFine = 0;										//The fine scrolling amount
	xWindowCoarse = windowStartCoarseX & 0x1F;				//In the tilemap, the left edge of the current positiom (coarse)
	xWindowBudToTile = (xWindowCoarse + (budStartCoarseX + 1)) & 0x1F;		//Where Bud is in relation to the onscreen portion of the name table (not the level map) to detect platforms etc
		
	xStripLeft = xWindowCoarse - budStartCoarseX;	//6				//Set the left draw edge in the tilemap
	
	if (xStripLeft < 0) {							//Handle negative rollover
		xStripLeft += 32;
	}
	
	xStripRight = ((xStripLeft + 26) & 0x1F);		//Compute right edge
	
	xLevelStripLeft = windowStartCoarseX - budStartCoarseX;		//Strip redraw position on left (when moving left)
	xLevelCoarse = windowStartCoarseX;				//In the tilemap, the left edge of the current positiom (coarse)	
	xLevelStripRight = xLevelStripLeft + 26;	//6	//Strip redraw position on right (when moving right)

}

void hallwayLogic() { //--------------------This is called at 30Hz. Your main game state machine should reside here

	if (displayPauseState == true) {			//Pause? Do no logic
		return;
	}
	
	highestObjectIndex = 24;					//3 windows + 3 greenies + 6 chandeliers + 7 robots max = 19 objects. Put windows last so robots appear in front of them

	stopWatchTick();

	//drawSpriteDecimal(minutes, 80, 0, 4);
	//drawSpriteDecimal(seconds, 100, 0, 6);
	// drawSpriteDecimal(robotKillTotal, 80, 16, 0);
	// drawSpriteDecimal(propDamageTotal, 80, 24, 0);

	// drawSpriteDecimal(kittenFloor, 40, 8, 0);
	// drawSpriteDecimal(robotKillFloor, 40, 16, 0);
	// drawSpriteDecimal(propDamageFloor, 40, 24, 0);


	if (budState != dead) {						//Messages top sprite pri, but don't draw if Bud dying
		if (kittenMessageTimer > 0) {
			if (kittenMessageTimer & 0x04) {
				drawSpriteText("GOTO ELEVATOR", 8, 56, 3);
			}	
			kittenMessageTimer--;
		}
	}

	//drawSpriteDecimalRight(score, 107, 4, 0);		//Right justified score
	drawBudStats();

	if (budState == entering) {		//If Bud is entering elevator or condo, draw robots first so they appear in front of him
		objectLogic();		
	}

	if (budSpawned == true) {
		budLogic2();
	}
	else {
		if (++budSpawnTimer == 20) {
			budSpawned = true;
			jump = 6;
			velocikitten = 5;

			object[21].sheetX = 0;					//Break the existing window
			playAudio("audio/glass_2.wav", 100);

			placeObjectInMap(32, 40, 0, 32 + 8, 4, 3, 6, true, 10, 6, 22);		//Spawn broken glass as object #22
			object[22].subAnimate = 0;
			object[22].animate = 0;
			object[22].moving = true;
		}
	}

	//thingLogic();
	//setWindow((xWindowCoarse << 3) | xWindowFine, yPos);			//Set scroll window
	setWindow((xWindowCoarse << 3) | xWindowFine + xShake, yPos + yShake);			//Set scroll window
	
	
	moveJump = false;

	// drawSpriteDecimal(xWindowCoarse, 10, 0, 0);
	// drawSpriteDecimal(xWindowBudToTile, 10, 8, 0);		
	// drawSpriteDecimal(budWorldX, 10, 16, 0);
	// drawSpriteDecimal(budWorldY, 10, 24, 0);	
	

	
	//drawSpriteDecimal(highestObjectIndex, 64, 0, 0);		
	// drawSpriteDecimal(xWindowBudFine, 8, 8, 0);	
	// drawSpriteDecimal(xWindowBudToTile, 8, 16, 0);
	// drawSpriteDecimal(xLevelStripRight, 8, 48, 0);		

	if (robotsToSpawn > 0) {
	
		if (--robotsSpaceTimer == 0) {
			
			robotsSpaceTimer = 90 + random(0, 50);			//Reset timer
			robotsToSpawn--;				//Dec #
			
			int x = 300;				//Robot spawns on left of map, facing right
			bool dir = false;
			
			if (budWorldX < 512) {		//If Bud on left side of map, spawn on right
				x = 700;
				dir =  true;
			}
			
			spawnHallwayRobot(x, dir);
			
			//int robot = placeObjectInMap(x, 64, 0, 48 + 0, 2, 6, random(4, 8), dir, 0, 0);
			
			//ADD RANDOM ROBOTS BJH

			// object[robot].speedPixels = robotSpeed;
			
			// object[robot].xSentryLeft = 8;
			// object[robot].xSentryRight = 1080;	
			// object[robot].moving = true;		//Move robot
			
		}
	}

	if (budState != entering) {		//Default draw Bud first so he appears over things like chandeliers
		objectLogic();		
	}
	else {
		if (waitPriorityChange == true) {		//Bud's state just changed, and we need one more frame before flipping priority?
			waitPriorityChange = false;			//Clear flag and...
			objectLogic();						//DEW IT			
		}
	}

}

void spawnHallwayRobot(int spawnXpos, bool direction) {

	selectRobot = random(0, 4);		//Get random robot #

	int x;

	for (x = 0 ; x < maxThings ; x++) {			//Find first open slot
		if (object[x].active == false) {
			objectIndexNext = x;
			break;
		}				
	}
	
	if (x == maxThings) {			//Didn't find a slot? Abort
		makeMessage("OBJECT LIMIT");
		messageTimer = 40;			
		return;
	}

	object[objectIndexNext].active = true;
	
	object[objectIndexNext].sheetX = robotSheetIndex[selectRobot][0];
	object[objectIndexNext].sheetY = robotSheetIndex[selectRobot][1] + 48;
	object[objectIndexNext].width = robotTypeSize[selectRobot][0];
	object[objectIndexNext].height = robotTypeSize[selectRobot][1];
	
	object[objectIndexNext].palette = random(4, 8);
	object[objectIndexNext].dir = direction;
	
	object[objectIndexNext].xSentryLeft = 8;
	object[objectIndexNext].xSentryRight = (hallwayWidth << 3) - 8; //1080; //sentryRight;

	object[objectIndexNext].xPos = spawnXpos;
	object[objectIndexNext].yPos = 112 - (object[objectIndexNext].height << 3);
	
	if (selectRobot == 2) {					//Flat robots don't fix an 8 pixel edge, drop by 4
		object[objectIndexNext].yPos += 4;
	}
	
	object[objectIndexNext].category = 0;					//0 = gameplay elements >0 BG stuff

	object[objectIndexNext].type = selectRobot;
	object[objectIndexNext].animate = 0;
	object[objectIndexNext].subAnimate = 0;
	

	object[objectIndexNext].speedPixels = robotSpeed;
	
	object[objectIndexNext].moving = true;		//Move robot


	highestObjectIndex++;

	// object[objectIndexNext].active = true;
	// object[objectIndexNext].state = 0;				//Default state
	
	// object[objectIndexNext].xPos = xPos;
	// object[objectIndexNext].yPos = yPos;
	// object[objectIndexNext].sheetX = sheetX;
	// object[objectIndexNext].sheetY = sheetY;
	// object[objectIndexNext].width = width;
	// object[objectIndexNext].height = height;
	// object[objectIndexNext].palette = palette;
	// object[objectIndexNext].dir = dir;

	// object[objectIndexNext].category = category;			//0 = gameplay elements >0 BG stuff
	// object[objectIndexNext].type = type;








	// objectSheetIndex[objectDropCategory][0] = robotSheetIndex[selectRobot][0];			//Get sheet XY and offset by page 3
	
	// if (selectRobot == 5) {			//Greenie?
		// objectSheetIndex[objectDropCategory][1] = robotSheetIndex[selectRobot][1] + 16;	
		// objectPlacePalette = 5;		//Always green
	// }
	// else {
		// objectSheetIndex[objectDropCategory][1] = robotSheetIndex[selectRobot][1] + 48;	
	// }
				
	// objectTypeSize[objectDropCategory][0] = robotTypeSize[selectRobot][0];
	// objectTypeSize[objectDropCategory][1] = robotTypeSize[selectRobot][1];	
	
	objectIndexLast = objectIndexNext;			//Store the index so when we complete the sentry entry we know where to store it

	
}


//Object scan------------------------------------
void objectLogic() {
	
	xShake = 0;
	yShake = 0;

	for (int g = 0 ; g < highestObjectIndex ; g++) {	//Scan no higher than # of objects loaded into level SCAN ALL? BJH
		
		if (object[g].active) {
			object[g].scan(worldX + xShake, 0 + yShake);					//Draw object if active, plus logic (in object) Robots move if off-camera

			if (object[g].state == 99 && object[g].moving == true) {						//Object falling?
				object[g].yPos += object[g].animate;
				
				if (object[g].animate < 8) {			//Object has to reach full fall speed in order to break or hit a robot
					object[g].animate++;				//Keep approaching TERMINAL VELOCITY
				}

				if (objectToTile(g) == true && object[g].animate > 6) {				//Scan base of object and see if it hit something solid (as wide as itself)
				//if (object[g].yPos > (119 - (object[g].height << 3))) {		//Object hit the floor?
					object[g].state = 100;					//Flag for main logic
					object[g].yPos &= 0xF8;					//Lob off remainder (tile-aligned)
					object[g].yPos += ((object[g].height - 1) << 3);	//Smash down to 1 tile high
					object[g].sheetX = 8 + random(0, 5);		//Turn object to rubble
					object[g].sheetY = 32 + 15;
					object[g].height = 1;						//Same width, height of 1 tile
					object[g].category = 99;					//Set to null type
				}
				
			}

			if (object[g].state == 100) {				//Object hit floor?
				if (object[g].type == 50) {				//Chandelier? Big sound
					playAudio("audio/glass_2.wav", 100);				
				}
				else {
					playAudio("audio/glass_loose.wav", 50);
				}
				fallListRemove(g);						//Remove it from active lists (also kills object)
				object[g].state = 101;					//Null state (rubble on floor, still active object)
			}

			if (object[g].visible) {

				if (object[g].category == 0) {
					if (object[g].type < 4) {				//Robot?
						
						switch(object[g].state) {
						
							case 200:						//Stunned, null state so robot can't hurt Bud during stun
								object[g].animate++;		//Could do this in object but I wanted something in this case
								break;
						
							case 201:
								playAudio("audio/explode.wav", 100);
								object[g].state = 202;
								object[g].animate = 0;
								object[g].subAnimate = 0;
								
								object[g].xPos += ((object[g].width << 3) / 2) - 8;			//Center explosion on object
								object[g].yPos += ((object[g].height << 3) / 2) - 8;
								
								object[g].category = 100;			//Explode!

								break;
								
							default:
								if (object[g].hitBox(budWx1, budWy1, budWx2, budWy2) == true && budBlink == 0 && budStunned == 0) {	//Did it hit Bud?
									budDamage();
								}
				
								fallCheckRobot(g);			//See if any falling objects hit this robot							
								break;
	
						}

					}	
				
					if (object[g].type == 4 && object[g].state == 0) {		//Un-rescued kitten?
						
						if (object[g].hitBoxSmall(budWx1, budWy1, budWx2, budWy2) == true) {
							score += 50;
							playAudio("audio/thankYou.wav", 99);	
							object[g].xPos += 4;			//Center rescuse kitten on sitting kitten
							object[g].yPos -= 4;
							object[g].state = 200;			//Rescue balloon
							object[g].animate = 0;
							kittenMessageTimer = 60;		//Show remain
							kittenCount++;					//Saved count
						}
						
					}	
					
					if (object[g].type == 5 && budPower < powerMax) {						//Greenie? Only run logic if Bud needs power (no pickup on full power)
						
						if (object[g].hitBoxSmall(budWx1, budWy1, budWx2, budWy2) == true) {	//EAT????
							score += 100;
							object[g].active = 0;
							playAudio("audio/greenie.wav", 90);
							if (++budPower > powerMax) {
								budPower = powerMax;
							}
						}
						
					}					
				}

				if (object[g].category == 100) {		//Is object exploding?
					
					xShake = random(0, 4);
					yShake = random(0, 4);
										
					if (object[g].active == false) {
						xShake = 0;
						yShake = 0;
					}
				}

				if (budState == swiping) {	//Bud is swiping?
					
					if (object[g].category > 0 && object[g].state == 0) {	//Bud swipes inert objects
						
						if (object[g].hitBox(budAx1, budAy1, budAx2, budAy2) == true) {
	
							if (currentMap == hallway) {
								propDamageFloor++;
							}
							else {
								propDamage++;
							}
							score += 2;

							fallListAdd(g);							//Add this item to the fall list
							
							playAudio("audio/slap.wav", 40);	
							object[g].state = 99;					//Falling
							object[g].animate = 1;
						}
						
					}
					
					if (object[g].category == 0) {						//Gameplay object?
						
						if (object[g].type == 4 && object[g].state == 0) {	//If bud hits a kitten...
							
							if (object[g].xSentryLeft == 0 && object[g].hitBox(budAx1, budAy1, budAx2, budAy2) == true) {
								
								score += 1;						//Score hack!
								
								object[g].xSentryLeft = 40;			//Cooldown for swat sounds
								
								kittenMeow = random(0, 4);
								
								switch(kittenMeow) {
									case 0:
										playAudio("audio/kitmeow0.wav", 30);
									break;
									
									case 1:
										playAudio("audio/kitmeow1.wav", 30);
									break;
									
									case 2:
										playAudio("audio/kitmeow2.wav", 30);
									break;
									
									case 3:
										playAudio("audio/kitmeow3.wav", 30);
									break;
								
								}

							}
							
						}							
						
						if (object[g].type < 4 && object[g].state == 200) {	//If a robot is in stun state

							if (object[g].hitBox(budAx1, budAy1, budAx2, budAy2) == true) {		//Punch landed? You're terminated!
								
								object[g].state = 201;			//State 100 = Stunned
								object[g].animate = 0;
								
								score += 125;
								
								if (currentMap == hallway) {
									robotKillFloor++;
								}
								else {
									robotKill++;
								}
									
							}								

						}						
						
					}
						
				}	
				
				if (object[g].category == 1 && jump > 0 && object[g].state == 0) {	//Bud can knock loose bad art and chandeliers by jumping through it
					
					if (object[g].hitBox(budWx1, budWy1, budWx2, budWy2) == true) {	
					
						if (currentMap == hallway) {
							propDamageFloor++;
						}
						else {
							propDamage++;
						}
							
						score += 1;								//Very low effort!	
							
						fallListAdd(g);							//Add this item to the fall list						
						playAudio("audio/slap.wav", 30);	
						object[g].state = 99;					//Falling
						object[g].animate = 1;
					}
					
				}

			}
	
		}
		
	}	
	
}


//Elevator (stage complete)-----------------------------------------------

void setupElevator() {
	
	music.StopTrack(currentFloor);			//End this floor's music (new floor = new music)
	
	stopAudio();
	
	clearObjects();

	loadPalette("condo/condo.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
	loadPattern("condo/condo.nes", 0, 256);			//Table 0 = condo tiles
	// loadPattern("sprites/bud.nes", 256, 256);		//Load bud sprite tiles into page 1	
	// loadPattern("sprites/objects.nes", 512, 256);	//Table 2 = condo objects
	// loadPattern("sprites/robots.nes", 768, 256);	//Table 3 = robots

	setButtonDebounce(up_but, true, 1);		//Debounce UP for door entry
	setButtonDebounce(down_but, false, 0);
	setButtonDebounce(left_but, false, 0);
	setButtonDebounce(right_but, false, 0);
	
	setCoarseYRollover(0, 14);   //Sets the vertical range of the tilemap, rolls back to 0 after 29

	setWindow(0, 0);

	fillTiles(0, 0, 14, 14, ' ', 0);			//Clear area

	//kittenFloor = 40;				//testing
	//robotKillFloor = 30;	
	//propDamageFloor = 50;
	//seconds = 48;
	//minutes = 1;
	
	kittenTotal += kittenFloor;					//Inc globals
	robotKillTotal += robotKillFloor;
	propDamageTotal += propDamageFloor;

	bonusPause = false;
	bonusPhase = 1;
	bonusTimer = 0;
	menuTimer = 1;



	//drawText("0123456789ABCDE", 0, 0, false);
	//drawText(" FLOOR X CLEAR", 0, 0, false);
	//drawDecimal(currentFloor, 7, 0);

	kittenMessageTimer = 0;			//Bug only in testing but fix it anyway

	kittyBonus = 0;
	robotBonus = 0;
	propBonus = 0;

	isDrawn = true;	
	displayPause(false);

	floorSeconds = (minutes * 60) + seconds;		//How much time we spent
	
	int bestTimePerLevel = (bestTime[currentFloor - 1][0] * 60) + bestTime[currentFloor - 1][1];	//Convert minutes-seconds to seconds
	
	timeBonus = calculate_score(floorSeconds, bestTimePerLevel, 10000);		//Award a % of max 10k based on speed
	
}

void elevatorLogic() {

	//fillTiles(0, 5, 14, 14, ' ', 0);			//Clear area

	if (bonusPause == true) {
		
		if (--menuTimer == 0) {
			bonusPhase++;
			bonusPause = false;			//Next frame jumps to next state
		}
		return;
	}

	switch(bonusPhase) {
	
		case 1:
			drawText(" KITTENS SAVED", 0, 0, false);
			drawDecimal(kittyBonus, 1);

			if (++bonusTimer > 1) {
				music.PlayTrack(15);
				bonusTimer = 0;
				if (kittyBonus == kittenFloor) {
					playAudio("audio/cash.wav", 100);
					menuTimer = 30;
					bonusPause = true;						
				}
				else {
					kittyBonus++;
					levelBonus += 100;				
				}
			}				

		break;	
	
		case 2:
			if (robotKillFloor == 0) {
						//0123456789ABCDE
				drawText("ROBOT PACIFIST", 0, 3, false);
				drawDecimal(10000 * currentFloor, 4);
				levelBonus += (10000 * currentFloor);
				playAudio("audio/cash.wav", 100);
				menuTimer = 30;
				bonusPause = true;			
			}
			else {
				drawText("ROBOTS  SMASHED", 0, 3, false);
				drawDecimal(robotBonus, 4);

				if (++bonusTimer > 1) {
					music.PlayTrack(15);
					bonusTimer = 0;
					if (robotBonus == robotKillFloor) {
						playAudio("audio/cash.wav", 100);
						menuTimer = 30;
						bonusPause = true;	
					}
					else {
						robotBonus++;
						levelBonus += 50;					
					}
				}		
			}
		break;

		case 3:
			if (propDamageFloor == 0) {
						//0123456789ABCDE
				drawText("PROPERTY MASTER", 0, 6, false);
				drawDecimal(20000 * currentFloor, 7);
				levelBonus += (20000 * currentFloor);
				playAudio("audio/cash.wav", 100);
				menuTimer = 30;
				bonusPause = true;				
			}
			else {
				drawText("PROPERTY DAMAGE", 0, 6, false);
				drawDecimal(propBonus, 7);
				
				if (++bonusTimer > 1) {
					music.PlayTrack(15);
					bonusTimer = 0;
					if (propBonus == propDamageFloor) {
						playAudio("audio/cash.wav", 100);
						menuTimer = 30;
						bonusPause = true;	
					}	
					else {
						propBonus++;			
						levelBonus += 10;					
					}
				}	
			}		
		break;
		
		case 4:
			drawText("  TIME BONUS   ", 0, 9, false);
			drawDecimal(timeBonus, 10);
			playAudio("audio/cash.wav", 100);				
			menuTimer = 60;
			bonusPause = true;	
		break;
		
		case 5:					
			score += levelBonus;		//Actually add the bonus to the score, then next level
			nextFloor();					
		break;		
		
		
	}
		
	drawText("  TOTAL SCORE", 0, 13, false);
	drawDecimal(score + levelBonus, 14);		//Draw score + bonus but don't actually add it til the end
	
	
}


void setupStory() {
	
	mapWidth = hallwayWidth;			//We used hallway mode to draw most of the story screens	
	stopAudio();	
	clearObjects();
	fillTiles(0, 0, 31, 31, ' ', 3);			//Clear whole area

	fileNameFloor = '1';		//Hallway hub is floor 1, condo 0
	fileNameCondo = '9';
	updateMapFileName();
	loadLevel();										//Load hallway level tiles, we will populate objects and redraw doors manually
				
	switch(whichScene) {
		
		case 0:
			loadPalette("story/reporter2.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
			loadPattern("story/reporter_t.nes", 0, 512);		//Table 0 = condo tiles + a few sprites for eye, hair and EXPLOSIONS!		

			drawStoryScreen(0);
	
			cutAnimate = 0;
			menuTimer = 120;
			break;
			
		case 1:
			loadPalette("story/rob_12.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
			loadPattern("story/rob_nice_t.nes", 0, 256);		//Table 0 = condo tiles + a few sprites for eye, hair and EXPLOSIONS!		

			drawStoryScreen(1);
	
			cutAnimate = 0;
			menuTimer = 70;
			break;			

		case 2:
			loadPalette("story/reporter2.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
			loadPattern("story/reporter_t.nes", 0, 512);		//Table 0 = condo tiles + a few sprites for eye, hair and EXPLOSIONS!		

			drawStoryScreen(0);
			fillTiles(0, 10, 14, 14, ' ', 3);
			cutAnimate = 0;
			cutSubAnimate = 19;
			cutSubAnimate2 = 0;
			explosionCount = 0;
			menuTimer = 120;
			break;
			
		case 3:
			loadPalette("story/rob_12.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
			loadPattern("story/rob_evil_t.nes", 0, 256);		//Table 0 = condo tiles + a few sprites for eye, hair and EXPLOSIONS!		

			drawStoryScreen(2);
	
			cutAnimate = 0;
			menuTimer = 70;
			break;				

		case 4:
			loadPalette("story/kitten.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
			loadPattern("story/kitten_t.nes", 0, 256);		//Table 0 = condo tiles + a few sprites for eye, hair and EXPLOSIONS!		

			drawStoryScreen(3);
			
			fillTiles(0, 10, 14, 14, ' ', 3);
	
			cutAnimate = 0;
			menuTimer = 140;
			break;	

			
		case 5:
			loadPalette("story/bud_face.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
			loadPattern("story/bud_face_t.nes", 0, 256);			//Table 0 = condo tiles		
			
			fillTiles(0, 0, 31, 31, ' ', 3);
			
			for (int y = 0 ; y < 10 ; y++) {
				
				for (int x = 0 ; x < 12 ; x++) {
					drawTile(x, y, x, y + 6, 0, 0);
				}
			
			}
					//0123456789ABCDE
			drawText(" WHO WILL SAVE", 0, 11, false);
			drawText(" THE KITTENS?", 0, 12, false);
			
			worldX = 96;
			menuTimer = 65;
			
			//music.StopTrack(0);		//Kill intro track
			//music.PlayTrack(15);	//Interlude
			break;
		
	}
	
	
	setCoarseYRollover(0, 14);   //Sets the vertical range of the tilemap, rolls back to 0 after 29
	setWindow(0, 0);
	isDrawn = true;	
	displayPause(false);
		
	
}

void storyLogic() {

	switch(whichScene) {
		
		case 0:
			drawSprite(16, 16, 4, 16, 3, 4, 4, false, false);	//Report eyes and hair glint
			
			cutAnimate++;
			
			if (cutAnimate == 8) {
				drawTile(3, 5, 0, 0, 0, 0);
				drawTile(4, 5, 1, 0, 0, 0);
				drawTile(3, 6, 0, 1, 0, 0);
				drawTile(4, 6, 1, 1, 0, 0);				
			}
			if (cutAnimate == 16) {
				drawTile(3, 5, 2, 0, 0, 0);
				drawTile(4, 5, 3, 0, 0, 0);
				drawTile(3, 6, 2, 1, 0, 0);
				drawTile(4, 6, 3, 1, 0, 0);	
				cutAnimate = 0;
			}
			menuTimer--;
			
			if (menuTimer == 119) {
				fillTiles(0, 10, 14, 14, ' ', 3);			//Clear text area, white palette
					        //0123456789ABCDE
					drawText("WELCOME TO THE", 0, 11, false);
					drawText(" CONDO OF THE", 0, 12, false);
					drawText("   FUTURE!", 0, 13, false);
			}
			if (menuTimer == 60) {
				fillTiles(0, 10, 14, 14, ' ', 3);			//Clear text area, white palette
					        //0123456789ABCDE
					drawText(" IT IS STAFFED", 0, 11, false);
					drawText(" ENTIRELY BY...", 0, 12, false);
			}		
			if (menuTimer == 0) {
				whichScene = 1;
				switchGameTo(story);
			}
		
			break;
			
		case 1:
			menuTimer--;
			
			if (menuTimer == 69) {
				fillTiles(0, 10, 14, 14, ' ', 3);			//Clear text area, white palette
					        //0123456789ABCDE
					drawText("SUPER FRIENDLY", 0, 11, false);
					drawText(" WORKER ROBOTS!", 0, 12, false);

			}
			if (menuTimer == 0) {
				whichScene = 2;
				switchGameTo(story);				
				
			}
		
			break;			
	
		case 2:
			drawSprite(16, 16, 4, 16, 3, 4, 4, false, false);	//Report eyes and hair glint
			
			cutAnimate++;
			
			if (cutAnimate == 8) {				//Scared face
				drawTile(3, 5, 4, 0, 0, 0);
				drawTile(4, 5, 5, 0, 0, 0);
				drawTile(3, 6, 4, 1, 0, 0);
				drawTile(4, 6, 5, 1, 0, 0);				
			}
			if (cutAnimate == 16) {
				drawTile(3, 5, 6, 0, 0, 0);
				drawTile(4, 5, 7, 0, 0, 0);
				drawTile(3, 6, 6, 1, 0, 0);
				drawTile(4, 6, 7, 1, 0, 0);	
				cutAnimate = 0;
			}
			
			if (++cutSubAnimate == 20 && explosionCount < 6) {
				cutSubAnimate = 0;
				budX = random(72, 96);
				budY = random(8, 44);
				cutSubAnimate2 = 8;
				playAudio("audio/bomb.wav", 100);
				explosionCount++;
			}
				
			if (cutSubAnimate2 > 0) {

				drawSprite(budX, budY, cutSubAnimate2, 16, 2, 2, 3, false, false);
				
				if (menuTimer & 0x01) {			//Advance explosion graphic on 2's
					cutSubAnimate2 += 2;
					if (cutSubAnimate2 == 14) {	//3 frames done? End (until respawn)
						cutSubAnimate2 = 0;
					}
				}
			}

			
			menuTimer--;
			
			if (menuTimer == 100) {
				fillTiles(0, 10, 14, 14, ' ', 3);			//Clear text area, white palette
					        //0123456789ABCDE

					drawText("    OH NO!", 0, 12, false);

			}
			if (menuTimer == 60) {
				fillTiles(0, 10, 14, 14, ' ', 3);			//Clear text area, white palette
					        //0123456789ABCDE
					drawText(" THE WORKER", 0, 11, false);
					drawText("  ROBOTS!...", 0, 12, false);
			}		
			if (menuTimer == 0) {
				whichScene = 3;
				switchGameTo(story);
			}
		
			break;

		case 3:		
			menuTimer--;
			
			if (menuTimer == 69) {
				fillTiles(0, 10, 14, 14, ' ', 3);			//Clear text area, white palette
					        //0123456789ABCDE
					drawText("THEY'VE TURNED", 0, 11, false);
					drawText("    EVIL!!!", 0, 12, false);
			}
			if (menuTimer == 0) {
				whichScene = 4;
				switchGameTo(story);							
			}
			break;			
	
		case 4:		
			menuTimer--;
			
			if (menuTimer == 139) {
				fillTiles(0, 10, 14, 14, ' ', 3);			//Clear text area, white palette
					        //0123456789ABCDE
					drawText("WE ARE GETTING", 0, 11, false);
					drawText(" REPORTS...", 0, 12, false);
					playAudio("audio/kitmeow0.wav", 100);
			}
			if (menuTimer == 80) {
				fillTiles(0, 10, 14, 14, ' ', 3);			//Clear text area, white palette
					        //0123456789ABCDE
					drawText(" THAT INNOCENT", 0, 11, false);
					drawText("  KITTENS ARE", 0, 12, false);
					drawText("TRAPPED INSIDE!", 0, 13, false);	
					playAudio("audio/kitmeow1.wav", 100);					
			}			
			if (menuTimer == 0) {
				whichScene = 5;
				switchGameTo(story);							
			}
			break;		
			
		case 5:
			if (worldX > 0) {
				worldX -= 4;
			}			
			
			for (int row = 0 ;  row < 10 ;  row++) {
				setWindowSlice(row, worldX);
			}

			menuTimer--;
			
			if (menuTimer == 35) {
				for (int y = 0 ; y < 3 ; y++) {
					
					for (int x = 0 ; x < 4 ; x++) {
						drawTile(x + 5, y + 3, x + 12, y + 9, 0, 0);
					}
				
				}				
			}
			
			if (menuTimer == 0) {
				music.StopTrack(0);		//Kill intro track
				switchGameTo(game);
			}
			
			break;
		
	}

	if (button(A_but)) {
		music.StopTrack(0);		//Kill intro track
		switchGameTo(game);
	}
	
}

void drawStoryScreen(int which) {
	
	int xCol = which * 15;
		
	for (int x = 0; x < 15 ; x++) {				//Draw 26 columns
		for (int y = 0 ; y < 15 ; y++) {							//Draw by row		
			tileDirect(x, y, condoMap[y][xCol + x]);
		}	
	}		
	
}	


void setupEnding() {

	stopAudio();
	
	clearObjects();

	loadPalette("story/ending_t.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
	loadPattern("story/ending_t.nes", 0, 256);			//Table 0 = condo tiles
	loadPattern("sprites/bud.nes", 256, 256);	//Table 1 = Bud Face sprites

	fillTiles(0, 0, 31, 31, ' ', 0);			//Black BG
			
	budX = 136;			//Use for 
	budY = 0;			//Use this for message #
	menuTimer = 1;	
	
	budFrame = 0;
	
	worldX = 136;			//BG
	worldY = 136;			//Used for grass
	setWindow(0, 0);		//Text will appear on left side

	for (int g = 0 ; g < 32 ; g++) {		
		drawTile(g, 8, 0xE0, 3, 0);		//Draw strip
	}

	for (int g = 0 ; g < 32 ; g += 8) {		
		for (int hg = 0 ; hg < 8 ; hg++) {
			for (int h = 0 ; h < 7 ; h++) {
				drawTile(g + hg, h, 0x60 + hg + (h * 16), 2, 0);		//Draw BG buildings
			}
		}	
	}
	
	
	for (int hg = 0 ; hg < 5 ; hg++) {			//Draw titular condo
		for (int h = 0 ; h < 9 ; h++) {
			drawTile(24 + hg, h, 0x69 + hg + (h * 16), 3, 0);	
		}
	}	

	
	for (int g = 0 ; g < 31 ; g += 4) {		
		drawTile(g, 9, 0xF0, 1, 0);		//Draw grass
		drawTile(g + 1, 9, 0xF1, 1, 0);		//Draw grass
		drawTile(g + 2, 9, 0xF2, 1, 0);		//Draw grass
		drawTile(g + 3, 9, 0xF3, 1, 0);		//Draw grass
	}


	setButtonDebounce(left_but, false, 0);
	setButtonDebounce(right_but, false, 0);

	displayPause(false);   		//Allow core 2 to draw
	isDrawn = true;	

	music.PlayTrack(13, true);
	
}

void endingLogic() {

	static int scroller = 0;
	//static int worldX = 136;
	//static int worldY = 136;

	static int kittenAnimate = 0;

	bool animateBud = false;			//Bud is animated "on twos" (every other frame at 30HZ, thus Bud animates at 15Hz)
	
	if (++budSubFrame > 1) {
		budSubFrame = 0;
		if (++budFrame > 7) {
			budFrame = 0;
		}
		if (++kittenAnimate > 3) {
			kittenAnimate = 0;
		}
	}	
		
	for (int g = 0 ; g < 9 ; g++) {		//Scroll slices, leave bottom text static
		
		setWindowSlice(g, worldX);	
	}
	
	setWindowSlice(9, worldY);

	if (--worldY < 0) {
		worldY = 32;
	}
	
	if (++scroller > 2) {		//BG scrolls much slower
		scroller = 0;
		if (--worldX < 0) {
			worldX = 64;
		}
	}

	drawSprite(12, 59, 3, 16 + (budFrame << 1), 3, 2, 4, true, false);	//Bud walking

	drawSprite(40, 59, 12, 16 + (kittenAnimate << 1), 2, 2, 5, true, false);	//Kittens
	
	drawSprite(57, 59, 12, 16 + (kittenAnimate << 1), 2, 2, 7, true, false);	//Kittens
	
	drawSprite(77, 59, 12, 16 + (kittenAnimate << 1), 2, 2, 5, true, false);	//Kittens
	
	drawSprite(100, 59, 12, 16 + (kittenAnimate << 1), 2, 2, 4, true, false);	//Kittens	

	if (--menuTimer == 0) {

		fillTiles(0, 10, 14, 14, ' ', 3);			//Black BG white text

		switch(budY) {

			case 0:
				menuTimer = 30;
			break;
		
			case 1:	    //0123456789ABCDE	
				drawText("CONGRATULATIONS", 0, 11, false);
				drawText(" YOU DEFEATED", 0, 12, false);
				drawText("THE EVIL ROBOTS", 0, 13, false);	
				menuTimer = 150;
			break;
			
			case 2:	    //0123456789ABCDE	
				drawText(" BUD FOUND NEW", 0, 11, false);
				drawText("SAFE HOMES FOR", 0, 12, false);
				drawText(" THE KITTENS ", 0, 13, false);
				menuTimer = 150;
			break;
		
			case 3:	    //0123456789ABCDE	
				drawText(" HE WANTED TO", 0, 11, false);
				drawText(" KEEP ONE AS A", 0, 12, false);
				drawText("NEW BROTHER BUT", 0, 13, false);
				drawText("  BEN SAID NO", 0, 14, false);
				menuTimer = 150;
			break;
	
			case 4:	    //0123456789ABCDE	
				drawText("ROBOTS SMASHED:", 0, 11, false);
				drawDecimal(robotKillTotal, 12);
				menuTimer = 80;
			break;
			
			case 5:	    //0123456789ABCDE	
				drawText(" STUFF BROKEN:", 0, 11, false);
				drawDecimal(propDamageTotal, 12);
				menuTimer = 80;
			break;
			case 6:	    //0123456789ABCDE	
				drawText("KITTENS SAVED:", 0, 11, false);
				drawDecimal(kittenTotal, 12);
				menuTimer = 80;
			break;	
			case 7:	    //0123456789ABCDE	
				drawText("  TOTAL SCORE", 0, 11, false);
				drawDecimal(score, 12);
				menuTimer = 80;
			break;	
			case 8:	    //0123456789ABCDE	
				drawText("  THANKS FOR", 0, 11, false);
				drawText("  JOING OUR", 0, 12, false);
				drawText(" 2023 WORKSHOP!", 0, 13, false);			
				menuTimer = 80;
			break;	
			
			case 9:	    //0123456789ABCDE	
				drawText("  PRESS A TO", 0, 11, false);
				drawText("   CONTINUE", 0, 12, false);		
				menuTimer = 80;
			break;				
	
		}

		if (++budY > 9) {
			budY = 0;
		}
	}

		
	if (button(A_but)) {
		music.StopTrack(13);		
		switchGameTo(gameOver);				
	}

}


int calculate_score(int time_taken, int best_time, int maximum_score) {
    float percent_of_best_time = (float)best_time / time_taken;
    int scoreAI = (int)(percent_of_best_time * maximum_score);
    return scoreAI;
}



void nextFloor() {

	if (++currentFloor > 6) {
		switchGameTo(theEnd);
		return;					//a winner is YOU
	}
	
	for (int x = 0 ; x < 6 ; x++) {		//Clear next set of condos
		apartmentState[x] = 0;
	}
	levelComplete = false;
	
	currentCondo = 0;			//Bud hasn't entered a condo

	budPower = powerMax;
	hallwayBudSpawn = 10;		//Bud from elevator

	levelBonus = 0;
	
	kittenFloor = 0;				//Total per floor (added to on condo clear)			
	propDamage = 0;					//Per condo
	propDamageFloor = 0;			//Total per floor (added to on condo clear)
	robotKill = 0;					//Per condo
	robotKillFloor = 0;				//Total per floor (added to on condo clear)	

	levelScore = score;				//Save last score at level (when saving, this is what you revert to)

	stopWatchClear();
	
	switchGameTo(goHallway);
	
}

void drawBudStats() {

	drawSprite(4, 4, 0x01F8, 4, false, false);	//face
	drawSprite(12, 4, 0x01F9, 0, false, false);	//X
	drawSpriteDecimal(budLives, 20, 4, 0);		//lives

	for (int x = 0 ; x < powerMax ; x++) {
		if (budPower > x) {
			drawSprite((x << 3) + 4, 14, 0x01FB, 4, false, false);
		}
		else {
			drawSprite((x << 3) + 4, 14, 0x01FA, 4, false, false);
		}			
	}		
	
}	

void budDamage() {

	//return;		//UNVINCIBLE BUD!!!!!!!!!!!!!!!!!

	if (budState == entering || budState == exiting) {
		return;
	}

	budPower--;			//Dec power

	//budDir = !budDir;

	if (budPower == 0) {	
		music.StopTrack(currentFloor);				
		budStunned = 10;				//Prevent object re-triggers during death
		menuTimer = 30;					//One second
		playAudio("audio/buddead.wav", 100);
		budState = dead;
		movingObjects(false);			//Freeze objects
		return;		
	}
	else {
		budStunned = 10;				//15 frame stun knockback then 1 sec invinc
		budStunnedDir = !budDir;
		playAudio("audio/budhit.wav", 100);
		budState = damaged;
		budFrame = 0;
	}
	
}

void fallListClear() {
	
	for (int x = 0 ; x < 16 ; x++) {
		fallingObjectState[x] = false;			//Set it active
		fallingObjectIndex[x] = 255;			//and set which object it's connected to
	}
	
}

void fallListAdd(int index) {		//Pass in object to fall

	for (int x = 0 ; x < 16 ; x++) {
		
		if (fallingObjectState[x] != true) {		//Find firt empty slot
		
			fallingObjectState[x] = true;			//Set it active
			fallingObjectIndex[x] = index;			//and set which object it's connected to
			// Serial.print("Fall list add #");
			// Serial.print(x);
			// Serial.print(" = index #");
			// Serial.println(index);
			
			break;							//Done
		}

	}

	
}

void fallListRemove(int index) {		//Pass in falling object to remove
	
	for (int x = 0 ; x < 16 ; x++) {

		if (fallingObjectState[x] == true) {			//Slot was set?
			
			if (fallingObjectIndex[x] == index) {		//is this the one we're looking for?
			
				//object[index].active = false;			//End object referenced (actually let's do this elsewhere/keep it as rubble)
				fallingObjectState[x] = false;			//Remove it from fall list
				
				// Serial.print("Fall list remove #");
				// Serial.print(x);
				// Serial.print(" = index #");
				// Serial.println(index);
				
				break;
			}	
			
		}

	}
	
}

void fallCheckRobot(int index) {		//Pass in robot object index we are checking for falls against

	for (int x = 0 ; x < 16 ; x++) {		
		if (fallingObjectState[x] == true) {		//Object is falling?
		
			// Serial.print("Fall list check ");
			// Serial.print(x);
			// Serial.print(" - ");
			// Serial.println(fallingObjectIndex[x]);
					
			int g = fallingObjectIndex[x];			//Get falling object index # (to make this next part not super long)
		
			if (object[g].animate > 7) {			//Object must be falling at at least half speed to kill a robot
			
				bool hit = false;
				
				// if (currentMap == condo) {				
					// if (object[index].hitBox(object[g].xPos, object[g].yPos, object[g].xPos + (object[g].width << 3), object[g].yPos + (object[g].height << 3)) == true) {
						// hit = true;
					// }
				// }
				// else {
				if (object[index].hitBoxSmall(object[g].xPos, object[g].yPos, object[g].xPos + (object[g].width << 3), object[g].yPos + (object[g].height << 3)) == true) {
					hit = true;
				}
				if (object[index].hitBox(object[g].xPos, object[g].yPos, object[g].xPos + (object[g].width << 3), object[g].yPos + (object[g].height << 3)) == true) {
					hit = true;
				}	

							
				if (hit == true) {
					score += 25;
					playAudio("audio/glass_1.wav", 90);	//Just the start of this (probably mix with object smash)
					fallingObjectState[x] = false;		//Remove it from fall list
					object[g].active = false;			//Destory falling object				
					object[index].state = 200;			//Short circuit stun robot
					object[index].stunTimer = stunTimeStart;
					object[index].animate = 0;
				}			
			}
	
		}
	}	
	
}

void movingObjects(bool state) {
	
	for (int x = 0 ; x < maxThings ; x++) {			//Find first open slot
		if (object[x].active == true) {				//If object exists and is a robot, turn movement on/off
			object[x].moving = state;
			
			if (difficulty == 3) {
				object[x].speedPixels = 2;
			}
			else {
				object[x].speedPixels = 1;
			}
			
			
		}
	}	
	
}

int objectToTile(int g) {		//Returns the tile flags nearest the given global world x y position. Allows objects to land on platforms other than the floor
	
	int x = object[g].xPos >> 3;		//Pixels to tiles
	int y = object[g].yPos >> 3;

	int edgeCount = 0;
	
	for (int i = 0 ; i < object[g].width ; i++) {			//Object must hit an edge along its entire width to land (so edge of counter it keeps going)	
		if ((condoMap[y + (object[g].height - 1)][x + i] & 0xF800) != 0) {			//Plat or block bits?
			edgeCount++;									//Inc count
		}		
	}
	
	if (edgeCount == object[g].width) {
		return true;
	}
	
	return false;

}


void budLogic2() {

	bool animateBud = false;			//Bud is animated "on twos" (every other frame at 30HZ, thus Bud animates at 15Hz)
	
	if (++budSubFrame > 1) {
		budSubFrame = 0;
		animateBud = true;				//Set animate flag. Bud still moves/does logic on ones
	}	

	if (budState == dead) {
		if (--menuTimer == 0) {
			switchGameTo(goJail);
		}	
	
		if (budStunnedDir == true) {	//Stunned rolling left
			drawSprite(budX, budY - 8, 14, 16 + (budFrame << 1), 2, 2, budPalette, false, false);	//Rolls left, but is facing right, rolling backwards
		}
		else {							//Stunned rolling right
			drawSprite(budX, budY - 8, 14, 16 + (budFrame << 1), 2, 2, budPalette, true, false);			
		}	
		
		if (menuTimer > 20) {
			return;
		}		
		
		
		if (animateBud) {
			if (++budFrame > 3) {
				budFrame = 0;
			}					
		}		
		
		bool centered = true;
		
		if (budX < 52) {
			budX += 2;
		}
		if (budX > 52) {
			budX -= 2;
		}
		if (budY < 60) {
			budY += 2;
		}
		if (budY > 60) {
			budY -= 2;
		}
		
		return;
	}

	budPalette = 4;						//Default bud palette. Change be changed to white to make him invinci-blink
	
	if (budBlink > 0) {
		if (budBlink > 15) {
			if (budBlink & 0x01) {				//If blink active, use alt Bud palette on ones (invinc flash)
				budPalette = 8;
			}				
		}
		else {
			if (budBlink & 0x02) {				//Blink slower when time almost up
				budPalette = 8;
			}				
		}
		budBlink--;						//Dec the invinc time		
	}
	
	int jumpGFXoffset = 6;						//Jumping up	
	bool moveSuppress = false;					//Default states changed by Bud action
	bool budNoMove = true;

	switch(budState) {
	
		case rest:								//Rest = not moving left or right. Can still be jumping or falling
			if (jump > 0) {
				int offset = 6;					//Jumping up gfx (default)
				
				if (jump == 128) {
					offset = 8;					//Falling down gfx
				}			
				if (budDir == false) {														//Falling facing right	
					drawSprite(budX, budY - 8, 0, 16 + offset, 3, 2, budPalette, budDir, false);
					setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 24, budWorldY + 8);
				}
				else {				
					drawSprite(budX - 8, budY - 8, 0, 16 + offset, 3, 2, budPalette, budDir, false);	//Falling facing left
					setBudHitBox(budWorldX - 8, budWorldY - 8, budWorldX + 16, budWorldY + 8);
				}				
			}
			else {
				setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 16, budWorldY + 6);			//2x2 tile hit box (his ears don't count)
	
				if (budDir == false) {			//Right
					drawSprite(budX, budY, 0 + tail, 31, budPalette, budDir, false);		//Draw animated tail on fours		
					drawSprite(budX + 8, budY, 7, 18, budPalette, budDir, false);		//Front feet
					drawSprite(budX, budY - 8, 6, 17, budPalette, budDir, false);		//Back

					if (mewTimer > 0) {
						mewTimer--;
						drawSprite(budX + 8, budY - 16, 7, 16 + 8, budPalette, budDir, false);	//Ear tips		
						drawSprite(budX + 8, budY - 8, 7, 16 + 9, budPalette, budDir, false);	//Mew mouth
					}
					else {
						if (blink < 35) {
							drawSprite(budX + 8, budY - 8, 7, 17, budPalette, budDir, false);
						}
						else {
							drawSprite(budX + 8, budY - 8, 6, 16, budPalette, budDir, false);	//Blinking Bud!
						}

						drawSprite(budX + 8, budY - 16, 7, 16, budPalette, budDir, false);	//Ear tips					
					}
				}
				else {							//Left
					drawSprite(budX + 8, budY, 0 + tail, 31, budPalette, budDir, false);		//Draw animated tail on fours		
					drawSprite(budX, budY, 7, 18, budPalette, budDir, false);		//Front feet
					drawSprite(budX + 8, budY - 8, 6, 17, budPalette, budDir, false);		//Back
	
					if (mewTimer > 0) {
						mewTimer--;
						drawSprite(budX, budY - 16, 7, 16 + 8, budPalette, budDir, false);	//Ear tips		
						drawSprite(budX, budY - 8, 7, 16 + 9, budPalette, budDir, false);	//Mew mouth
					}
					else {
						if (blink < 35) {
							drawSprite(budX, budY - 8, 7, 17, budPalette, budDir, false);
						}
						else {
							drawSprite(budX, budY - 8, 6, 16, budPalette, budDir, false);	//Blinking Bud!
						}

						drawSprite(budX, budY - 16, 7, 16, budPalette, budDir, false);	//Ear tips				
					}
				}	
				if (animateBud) {
					if (++tail > 2) {
						tail = 0;
					}
					if (++blink > 40) {
						blink = 0;
					}
				}				
			}
			break;
		
		case moving:
			if (jump > 0) {						//If jumping while moving use the jump stills
				int offset = 6;					//Jumping up gfx (default)
				if (jump == 128) {
					offset = 8;					//Falling down gfx
				}	
				if (budDir == false) {														//Falling facing right	
					drawSprite(budX, budY - 8, 0, 16 + offset, 3, 2, budPalette, budDir, false);
					setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 24, budWorldY + 6);
					budMoveRightC(3);
				}
				else {				
					drawSprite(budX - 8, budY - 8, 0, 16 + offset, 3, 2, budPalette, budDir, false);	//Falling facing left
					setBudHitBox(budWorldX - 8, budWorldY - 8, budWorldX + 16, budWorldY + 6);
					budMoveLeftC(3);
				}		
			}
			else {										//Running on ground animation
	
				if (budDir == false) {		
					drawSprite(budX, budY - 8, 0, 16 + (budFrame << 1), 3, 2, budPalette, budDir, false);	//Running
					setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 24, budWorldY + 6);
					budMoveRightC(3);		
				}
				else {				
					drawSprite(budX - 8, budY - 8, 0, 16 + (budFrame << 1), 3, 2, budPalette, budDir, false);	//Running
					setBudHitBox(budWorldX - 8, budWorldY - 8, budWorldX + 16, budWorldY + 6);
					budMoveLeftC(3);		
				}
				if (animateBud) {
					if (++budFrame > 6) {
						budFrame = 0;
					}					
				}		
			}
			break;	

		case swiping:
			if (budDir == true) {			//Left
				drawSprite(budX - 16, budY - 16, 8, 16 + (budSwipe * 3), 4, 3, budPalette, budDir, false);
				setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 16, budWorldY + 6);
				setBudAttackBox(budWorldX - 16, budWorldY - 12, budWorldX, budWorldY);
			}
			else {							//Right
				drawSprite(budX, budY - 16, 8, 16 + (budSwipe * 3), 4, 3, budPalette, budDir, false);
				setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 16, budWorldY + 6);
				setBudAttackBox(budWorldX + 16, budWorldY - 12, budWorldX + 32, budWorldY);
			}	
			break;

		case damaged:
			if (budStunnedDir == true) {	//Stunned rolling left
				drawSprite(budX, budY - 8, 14, 16 + (budFrame << 1), 2, 2, budPalette, false, false);	//Rolls left, but is facing right, rolling backwards
				setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 16, budWorldY + 6);
				budMoveLeftC(4);
			}
			else {							//Stunned rolling right
				drawSprite(budX, budY - 8, 14, 16 + (budFrame << 1), 2, 2, budPalette, true, false);
				setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 16, budWorldY + 6);
				budMoveRightC(4);				
			}
			if (animateBud) {
				if (++budFrame > 3) {
					budFrame = 0;
				}					
			}
			//stunned count was here
			break;

		case exiting:			//Coming OUT of a condo or elevator and going INTO hallway	
			moveSuppress = true;		
			if (budVisible == true) {
				drawSprite(budX + 4, budY - 8, 7, 16 + 5, 1, 2, 4, budDir, false);	
			}			
			if (animateBud) {
				if (budFrame & 0x02) {
					budDir = false;
				}					
				else {
					budDir = true;
				}				
				if (budExitingWhat == 1) {									//Exiting a condo?			
					if (++budFrame > 10) {
						budFrame = 0;
						budState = rest;
						budDir = false;
						apartmentState[currentCondo - 1] = 0x80;				//Set door closed, apt clear
						playAudio("audio/doorOpen.wav", 90);
						drawHallwayDoorClosed(doorTilePos[currentCondo - 1], currentCondo);
						//populateDoor(doorTilePos[currentCondo - 1], currentCondo, false);		//CLOSE THE DOOR ALEX THERE'S A DRAFT!				
						redrawMapTiles();
						//redrawCurrentHallway();	
					}										
				}
				else {					
					budFrame++;					
					if (budFrame == 5) {
						budVisible = true;					//He's visible once the doors open
						playAudio("audio/elevDing.wav", 100);
						elevatorOpen = true;				//Doors open, 10 frames on 2's open
						drawHallwayElevator(64);
						redrawMapTiles();					
					}									
					if (budFrame == 15) {					//then close, now Bud can move
						budFrame = 0;
						budState = rest;
						budDir = false;
						elevatorOpen = false;	
						drawHallwayElevator(64);						
						redrawMapTiles();	
					}					
				}				
			}			
			break;
	
		case entering:					//Going into a condo or elevator
			moveSuppress = true;		
			drawSprite(budX + 4, budY - 8, 7, 16 + 3, 1, 2, 4, budDir, false);		
			if (animateBud) {
				if (budFrame & 0x02) {
					budDir = false;
				}					
				else {
					budDir = true;
				}
				if (++budFrame > 15) {
					budFrame = 0;
					if (levelComplete == true) {
						switchGameTo(goElevator);
					}
					else {
						switchGameTo(goCondo);
					}					
				}
			}				
			break;

	}

	if (budStunned > 0) {		
		if (--budStunned == 0) {
			budState = rest;
			budBlink = 30;			//After the stun 1 sec invincible
		}			
	}


	if (moveSuppress == false) {			//If Bud isn't frozen (enter/exit/dead) then player can move him
		
		if (jump & 0x7F) {					//Jump is set, but the MSB (falling) isn't? Must be rising still
			if (++jump < 12) {				//Jump up with decreasing velocity	
				budY -= velocikitten;
				budWorldY -= velocikitten;
				if (velocikitten > 1) {				
					velocikitten--;
				}
			}
			else {							//Hit top? Set falling and reset velocity
				jump = 128;
				velocikitten = 2;
			}
			
		}
		
		if (jump == 0 || jump == 128) {		//Walked off a ledge or coming down from a jump?
		
			bool fallEdge = false;
			
			
			if (budY > -1) {				//Only do checks with Bud on screen (else tile data is bad)
				if (getTileType(budTileCheck(0), (budY >> 3)) == 0 && getTileType(budTileCheck(1), (budY >> 3)) == 0) {	//Nothing below?
					fallEdge = true;
				}			
			}
			else {
				fallEdge = true;			//Bud is above top of screen so must be falling
			}
		
			if (fallEdge == true) {			//Nothing below?
			
				if (jump == 0) {			//Weren't already falling?
					velocikitten = 2;		//Means we walked off a ledge, reset velocity
					jump = 128;				//Set falling state
					//budState = jumping;
				}

				budY += velocikitten;					//Fall
				budWorldY += velocikitten;
				if (velocikitten < 7) {
					velocikitten++;
				}
			}
			else {
				if (jump == 128) {				//Were we falling? Clear flag
					jump = 0;					//Something below? Clear jump flag
					budY &= 0xF8;
					budWorldY &= 0xF8;
					
					if (budStunned == 0) {		//If falling while stunned, don't reset state (stun timeout will)
						budState = rest;
					}	
				}		
			}
		
		}

		if (button(C_but)) {
			if (jump == 0) {
				jump = 1;
				velocikitten = 9;
				budNoMove = false;
				//budState = jumping;	
			}
			
		}

		if (budSwipe < 99 && animateBud == true) {
			if (++budSwipe > 2) {
				budSwipe = 99;
				budState = rest;
			}
		}

		if (button(B_but) && jump == 0) {		//Can't swipe while jumping
		
			if (budSwipe == 99) {
				budSwipe = 0;	
				budState = swiping;
			}
				
		}
		
		if (button(left_but)) {
			budDir = true;
		
			switch(budState) {
			
				case rest:
					budState = moving;
					budFrame = 0;
					break;
					
				case moving:
					budDir = true;
					//bud moves xPos change
					break;	
					
				case jumping:
					moveJump = true;
					budDir = true;
					//bud moves xPos change
					break;				

				case stopping:
					budState = moving;
					break;
		
			}
			budNoMove = false;
			
		}
		
		if (button(right_but)) {
			budDir = false;
			
			switch(budState) {
			
				case rest:
					budState = moving;
					budFrame = 0;
					break;
					
				case moving:
					budDir = false;
					//bud moves xPos change
					break;

				case jumping:
					moveJump = true;
					budDir = false;
					//bud moves xPos change
					break;					

				case stopping:
					budState = moving;
					break;
		
			}
			budNoMove = false;

		}

		if (button(A_but) && mewTimer == 0) {
			playAudio("audio/bud_mew.wav", 90);
			mewTimer = 20;
			
			// levelComplete = true;
			// elevatorOpen = true;
			// drawHallwayElevator(64);
			// redrawMapTiles();
			// kittenMessageTimer = 60;		//In hallway logic, use this var to print GOTO THE ELEVATOR
		
			//kittenCount = kittenToRescue;	//Hackzor cheats
			
			//worldX = -2;
		}	
			
	}

	if (budNoMove == true && budState == moving) {		//Was Bud moving, but then d-pad released? Bud comes to a stop
		budState = rest;
		budFrame = 0;
	}
		
	if (currentMap == hallway) {					//In the hallway? See if Bud can enter a door or elevator
		
		int temp = checkDoors();			//See if Bud is standing in front of a door
		
		if (temp > -1 && budState != entering) {					//He is? tempCheck contains a index # of which one (0-5)
			
			if ((apartmentState[temp] & 0xC0) == 0x00) {	//If apartment cleared or door already open bits set, no action can be taken

				if (button(up_but)) {
					apartmentState[temp] |= 0x40;								//Set the door open bit
					playAudio("audio/doorOpen.wav", 100);
					drawHallwayDoorOpen(doorTilePos[temp], 0);
					redrawMapTiles();
					budState = entering;
					waitPriorityChange = true;		//Tells the hallwaylogic wrapper to draw sprite priority normal until next frame
					budFrame = 0;	
					currentCondo = temp + 1;
				}
				
				drawSprite(budX, budY - 28 - (inArrowFrame >> 1), 6, 16 + 14, 2, 1, 4, false, false);		//IN ARROW sprites			
				drawSprite(budX, budY - 20, 6, 16 + 15, 2, 1, 4, false, false);		//IN ARROW sprites
				
				if (++inArrowFrame > 7) {
					inArrowFrame = 0;
				}
		
			}
			
		}

		if (checkElevator() == 1 && budState != entering && budState != exiting) {				//Bud standing in front of elevator?
			
			if (elevatorOpen == true) {							//Ready for the next level?

				if (button(up_but)) {
					playAudio("audio/elevDing.wav", 100);
					budState = entering;
					waitPriorityChange = true;		//Tells the hallwaylogic wrapper to draw sprite priority normal until next frame
					budFrame = 0;				
				}
				
				drawSprite(budX, budY - 28 - (inArrowFrame >> 1), 6, 16 + 14, 2, 1, 4, false, false);		//IN ARROW sprites			
				drawSprite(budX, budY - 20, 6, 16 + 15, 2, 1, 4, false, false);		//IN ARROW sprites
				
				if (++inArrowFrame > 7) {
					inArrowFrame = 0;
				}
		
			}
			
		}
	
	}
		
	
	
	
}

bool budMoveLeftC(int speed) {

	if (budY > -1) {		//Tile obstacle check first
		if ((getTileType(budTileCheck(-1), (budY >> 3)) & tileBlocked) == tileBlocked) {		//Check 1 to the left
			return false;
		}		
	}

	if (condoMoveLeft(speed) == false) {		//Can window NOT scroll left?

		if (budY > 56) {			//Bud can move further left than 8px if he's inside the door (to exit or be taunted)
			budX -= speed;
			budWorldX -= speed;
			xWindowBudFine -= speed;
			
			if (xWindowBudFine < 0) {				//Passed tile boundary?
				xWindowBudFine += 8;			//Reset fine bud screen scroll
				
				xWindowBudToTile = windowCheckLeft(xWindowBudToTile);
			}	

			if (budX < 8 && currentMap == condo) {
				if (kittenCount == kittenToRescue) {		//Rescued enough to exit condo
					exitCondo();
				}
				else {
					if (kittenMessageTimer == 0) {
						kittenMessageTimer = 60;	
						playAudio("audio/taunt.wav", 100);
					}
				}
			}
		}
		else {

			if (budX > 8) {
				budX -= speed;
				budWorldX -= speed;
				xWindowBudFine -= speed;
				
				if (xWindowBudFine < 0) {				//Passed tile boundary?
					xWindowBudFine += 8;			//Reset fine bud screen scroll
					
					xWindowBudToTile = windowCheckLeft(xWindowBudToTile);
				}	
			}

		}


		

	}
	
	return true;
}

bool budMoveRightC(int speed) {

	if (budY > -1) {							//Tile obstacle check first
		if ((getTileType(budTileCheck(2), (budY >> 3)) & tileBlocked) == tileBlocked) {			//Check 2 to the right
			return false;
		}		
	}

	if (condoMoveRight(speed) == false) {		//Can window NOT scroll right?
		
		if (budX < 98) {			//Prevent from jumping over right edge of the world (DAMN SPEED RUNNERS!)
			budX += speed;
			budWorldX += speed;
			xWindowBudFine += speed;
			
			if (xWindowBudFine > 7) {				//Passed tile boundary?
				xWindowBudFine -= 8;			//Reset fine bud screen scroll
				
				xWindowBudToTile = windowCheckRight(xWindowBudToTile);
			}		
		}
	
	}
	
	return true;
}

bool condoMoveLeft(int theSpeed) {

	if (budX > 48) {
		return false;
	}
	
	xWindowFine -= theSpeed;				//Move fine scroll		
	
	budWorldX -= theSpeed;
	worldX -= theSpeed;
	
	if (xWindowFine < 0) {				//Passed tile boundary?
	
		if (xLevelCoarse == 0) {		//At left edge?
			xWindowFine = 0;			//Clear out fine register, we can't go further left
			worldX = 0;
			//budWorldX = 0;
			
			//Serial.println("Left edge");
			
			//worldX += theSpeed;
			budWorldX += theSpeed;
			return false;				//Bud can now move past his scroll edge
		}

		xWindowFine += 8;				//Subtract fine		
		
		xWindowCoarse = windowCheckLeft(xWindowCoarse);
		xWindowBudToTile = windowCheckLeft(xWindowBudToTile);
		xStripLeft = windowCheckLeft(xStripLeft);
		xStripRight = windowCheckLeft(xStripRight);

		xLevelStripLeft--;
		
		if (xLevelStripLeft < 0) {
			xLevelStripLeft += mapWidth;
		}
		
		xLevelCoarse--;
		
		xLevelStripRight--;		
		
		if (xLevelStripRight < 0) {
			xLevelStripRight += mapWidth;
		}

		xStripDrawing = xStripLeft;			//Where to draw new tiles on tilemap
			
		if (currentMap == hallway) {
			//drawHallwayTiles(xLevelStripLeft);				//Draw the strip
			drawCondoStrip(xLevelStripLeft, xStripLeft);	//Draw the hallway
		}
		else {
			drawCondoStrip(xLevelStripLeft, xStripLeft);	//Draw the hallway
		}

	}	
	
	return true;
		
}

bool condoMoveRight(int theSpeed) {
	
	if (budX < 56) {
		return false;
	}
	
	budWorldX += theSpeed;
	xWindowFine += theSpeed;				//Move fine scroll		
	
	worldX += theSpeed;	
	
	if (xWindowFine > 7) {				//Passed tile boundary?
	
		if (xLevelCoarse == (mapWidth - 16)) {		//At right edge?
			xWindowFine = 7;			//Max out fine register, we can't go further right
			worldX -= theSpeed;
			budWorldX -= theSpeed;			
			return false;				//Bud can now move past his scroll edge
		}
	
		xWindowFine -= 8;				//Subtract fine		
		
		xWindowCoarse = windowCheckRight(xWindowCoarse);
		xWindowBudToTile = windowCheckRight(xWindowBudToTile);
		xStripLeft = windowCheckRight(xStripLeft);
		xStripRight = windowCheckRight(xStripRight);

		xLevelStripLeft++;
		
		if (xLevelStripLeft == mapWidth) {
			xLevelStripLeft -= mapWidth;
		}
		
		xLevelCoarse++;
		
		xLevelStripRight++;		
		
		if (xLevelStripRight == mapWidth) {
			xLevelStripRight -= mapWidth;
		}

		xStripDrawing = xStripRight;			//Where to draw new tiles on tilemap
	
		if (mapWidth == hallwayWidth) {
			//drawHallwayTiles(xLevelStripRight);	//Draw the strip
			drawCondoStrip(xLevelStripRight, xStripRight);
		}
		else {
			drawCondoStrip(xLevelStripRight, xStripRight);
		}

	}

	return true;
	
}

int windowCheckLeft(int theValue) {
	
	if (--theValue < 0) {
		theValue = 31;
	}
	
	return theValue;	
	
}

int windowCheckRight(int theValue) {
	
	if (++theValue > 31) {
		theValue = 0;
	}
	
	return theValue;		
	
}

int budTileCheck(int whereTo) {

	int temp = xWindowBudToTile;	//Get tile position

	if (whereTo < 0) {				//Negative? (checking left)
		temp += whereTo;
		
		if (temp < 0) {				//Acount for rollovers
			temp += 32;
		}
	}
	else {							//Positive (checking right)
		temp += whereTo;
		
		if (temp > 31) {
			temp -= 32;
		}		
	}

	return temp;	
	
}

void setBudHitBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {		//Low side -2 pixels so robots passing under don't kill Bud (his ears at rest also null)
	
	budWx1 = x1;
	budWy1 = y1;	
	budWx2 = x2;
	budWy2 = y2;	
}

void setBudAttackBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
	
	budAx1 = x1;
	budAy1 = y1;	
	budAx2 = x2;
	budAy2 = y2;
	
}


void stopWatchTick() {

	if (++frames == 30) {
		frames = 0;
		if (++seconds == 60) {
			seconds = 0;
			++minutes;				//I guess this is how you 'sploit in Angry Birds code injection?
		}			
	}

}

void stopWatchClear() {

	seconds = 0;
	minutes = 0;
	
}


int checkDoors() {
	
	uint16_t tileX = budWorldX >> 3;			//Convert Bud fine world X to tiles X

	if (budY != 104) {							//Doors only ping if Bud is on floor		
		return -1;
	}

	for (int x = 0 ; x < 6 ; x++) {
	
		if (tileX >= doorTilePos[x] && tileX <= (doorTilePos[x] + 1)) {			//Is Bud aligned with a door?
			return x;
		}
	
	}

	return -1;			//Not in front of a door
	
}

int checkElevator() {
	
	uint16_t tileX = budWorldX >> 3;			//Convert Bud fine world X to tiles X

	if (budY != 104) {							//Elevator only pings if Bud is on floor		
		return -1;
	}

	if (tileX >= 65 && tileX <= 68) {			//Is Bud aligned with elevator?
		return 1;
	}

	return -1;
	
}





void drawHallwayDoorClosed(int whichStrip, int whichDoorNum) {		//whichDoorNum is 1-6

	const char tiles[4][9] = {
		{ 0x99, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x90, 0xA0 },
		{ 0x9A, 0x94, 0x97, 0x94, 0x94, 0x9F, 0x94, 0x91, 0xA1 },	//By default it draws as number XX. If not cleared, draw floor-room # over this
		{ 0x9A, 0x94, 0x98, 0x94, 0x94, 0x9F, 0x94, 0x91, 0xA2 },		
		{ 0x9B, 0x95, 0x95, 0x96, 0x95, 0x95, 0x95, 0x92, 0xA3 },		
	};

	for (int col = 0 ; col < 4 ; col++) {
		drawTileMap(whichStrip, 14, 0x9E, 3, tileBlocked);		//Draw floor
		
		for (int g = 0 ; g < 8 ; g++) {
			drawTileMap(whichStrip, 13 - g, tiles[col][g], 0, 0);		//Draw door in strips
		}		
		setTileTypeMap(whichStrip, 13, tilePlatform);						//Bottom row is platform type
	
		if (col == 1) {		
			if (apartmentState[whichDoorNum - 1] == 0) {						//Apartment not clear? (MSB set)
				drawTileMap(whichStrip, 8, 0x80 + currentFloor, 0, 0);		//Draw floor #
			}
		}
		if (col == 2) {
			if (apartmentState[whichDoorNum - 1] == 0) {						//Apartment not clear? (MSB set)
				drawTileMap(whichStrip, 8, 0x80 + whichDoorNum, 0, 0);		//Draw condo # +1 because zero offset
			}			
		}	
	
		setTileTypeMap(whichStrip, 5, tilePlatform);						//Top sil is platform
	
		whichStrip++;
	
	}


	
}

void drawHallwayDoorOpen(int whichStrip, int whichDoorNum) {		//Hallway object 1

	const char tiles[4][9] = {
		{ 0x8E, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8A, 0xA0 },
		{ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x8B, 0xA1 },	
		{ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x8B, 0xA1 },		
		{ 0x8F, 0x8F, 0x8F, 0x8F, 0x8F, 0x8F, 0x8F, 0x8C, 0xA3 },		
	};

	for (int col = 0 ; col < 4 ; col++) {
		drawTileMap(whichStrip, 14, 0x9E, 3, tileBlocked);		//Draw floor
		
		for (int g = 0 ; g < 8 ; g++) {
			drawTileMap(whichStrip, 13 - g, tiles[col][g], 0, 0);		//Draw door in strips
		}		
		setTileTypeMap(whichStrip, 13, tilePlatform);						//Bottom row is platform type
		setTileTypeMap(whichStrip, 5, tilePlatform);						//Top sil is platform
		whichStrip++;
	
	}
		
}

void drawHallwayCounter(int whichStrip) {	//Hallway object 2

	const char tiles[4][5] = {
		{ 0xF0, 0xE0, 0xD0, 0xC0 },
		{ 0xF1, 0xE1, 0xD1, 0xC1 },	
		{ 0xF2, 0xE2, 0xD2, 0xC2 },		
		{ 0xF3, 0xE3, 0xD3, 0xC3 },		
	};

	for (int col = 0 ; col < 4 ; col++) {
		drawTileMap(whichStrip, 14, 0x9E, 3, tileBlocked);		//Draw floor
		
		for (int g = 0 ; g < 4 ; g++) {
			drawTileMap(whichStrip, 13 - g, tiles[col][g], 1, 0);
		}	
		
		setTileTypeMap(whichStrip, 13, tilePlatform);			//Cell above floor is plat
		
		drawTileMap(whichStrip, 10, tiles[col][3], 3, 0);		//Top is a dif pallete
		
		setTileTypeMap(whichStrip, 9, tilePlatform);			//Cell above is a plat

		whichStrip++;
		
	}
	
}

void drawHallwayCounterWindow(int whichStrip) {						//Counter + window (black BG covered with sprite)

	const char tiles[4][5] = {
		{ 0xF4, 0xE4, 0xD4, 0xC4 },
		{ 0xF5, 0xE5, 0xD5, 0xC5 },	
		{ 0xF6, 0xE6, 0xD6, 0xC6 },		
		{ 0xF7, 0xE7, 0xD7, 0xC7 },		
	};

	for (int col = 0 ; col < 4 ; col++) {
		drawTileMap(whichStrip, 14, 0x9E, 3, tileBlocked);		//Draw floor
		
		for (int g = 0 ; g < 4 ; g++) {
			drawTileMap(whichStrip, 13 - g, tiles[col][g], 1, 0);
		}	
		
		setTileTypeMap(whichStrip, 13, tilePlatform);			//Cell above floor is plat
		
		drawTileMap(whichStrip, 10, tiles[col][3], 3, 0);		//Top is a dif pallete
		
		setTileTypeMap(whichStrip, 9, tilePlatform);			//Cell above is a plat
	
		for (int g = 6 ; g < 9 ; g++) {
			drawTileMap(whichStrip, g, ' ', 3, 0);		    //Black hole for windows
		}

		whichStrip++;
		
	}

	
}

void drawHallwayElevator(int whichStrip) {							//Elevator (open or closed)

	char floorText[7] = { 0xDC, 0xDD, 0xDE, 0xDF, 0xDC, 0xDC, 0xDC };
	
	floorText[4] = currentFloor + 0x80;

	if (elevatorOpen == false) {
		const char tiles[8][10] = {
			{ 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xA0 },
			{ 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xA1 },	
			{ 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xA1 },	
			{ 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xA1 },
			{ 0xEC, 0xEC, 0xEC, 0xEC, 0xEC, 0xEC, 0xEC, 0xEC, 0xEC, 0xA1 },
			{ 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xA1 },	
			{ 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xA1 },		
			{ 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xA3 },		
		};

		for (int col = 0 ; col < 8 ; col++) {
			
			for (int g = 0 ; g < 9 ; g++) {
				drawTileMap(whichStrip, 13 - g, tiles[col][g], 3, 0);
			}	
			
			setTileTypeMap(whichStrip, 13, tilePlatform);
			
			drawTileMap(whichStrip, 4, tiles[col][9], 3, tilePlatform);	//Draw top sil of doorway as platform
	
			if (col > 0 && col < 7) {
				drawTileMap(whichStrip, 3, floorText[col - 1], 0, 0);
			}
	
			whichStrip++;
	
			
		}
					
	}
	else {
		const char tiles[8][10] = {
			{ 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xA0 },
			{ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xA1 },	
			{ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xA1 },
			{ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xA1 },
			{ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xA1 },	
			{ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xA1 },	
			{ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xA1 },	
			{ 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xA3 },		
		};

		for (int col = 0 ; col < 8 ; col++) {
			
			for (int g = 0 ; g < 9 ; g++) {
				drawTileMap(whichStrip, 13 - g, tiles[col][g], 3, 0);
			}	
			
			setTileTypeMap(whichStrip, 13, tilePlatform);
			
			drawTileMap(whichStrip, 4, tiles[col][9], 3, tilePlatform);	//Draw top sil of doorway as platform
	
			if (col > 0 && col < 7) {
				drawTileMap(whichStrip, 3, floorText[col - 1], 0, 0);
			}
			
			whichStrip++;
			
		}
	}

}




void drawTileMap(int xPos, int yPos, uint16_t whatTile, char whatPalette, int flags) {		//Same as the one in library but it draws to condomap memory
	
		flags &= 0xF8;							//You can use the top 5 bits for attributes, we lob off anything below that (the lower 3 bits are for tile palette)

		condoMap[yPos][xPos] = (flags << 8) | (whatPalette << 8) | whatTile;			//Palette points to a grouping of 4 colors, so we shift the palette # 2 to the left to save math during render		

}

void setTileTypeMap(int tileX, int tileY, int flags) {						
	
	flags &= 0xF8;							//You can use the top 5 bits for attributes, we lob off anything below that (the lower 3 bits are for tile palette)
	
	condoMap[tileY][tileX] &= 0x07FF;				//AND off the top 5 bits
	
	condoMap[tileY][tileX] |= (flags << 8);		//OR it into the tile in nametable
	
}




//-------LEVEL EDITOR-------------------------------------
void setupEdit() {

	clearObjects();
	
	loadPattern("sprites/bud.nes", 256, 256);
	loadPattern("sprites/objects.nes", 512, 256);	//Table 2 = condo objects
	loadPattern("sprites/robots.nes", 768, 256);	//Table 3 = robots
	
	setButtonDebounce(up_but, false, 0);			//Tile sliding debounce
	setButtonDebounce(down_but, false, 0);
	setButtonDebounce(left_but, false, 0);
	setButtonDebounce(right_but, false, 0);	
	
	setButtonDebounce(B_but, false, 0);
	setButtonDebounce(A_but, false, 0);	
	setButtonDebounce(C_but, true, 1);
	
	setCoarseYRollover(0, 14);   //Sets the vertical range of the tilemap, rolls back to 0 after 29

	editWindowX = 0;

	cursorMoveTimer = 0;
	
	currentFloor = 1;			//Default edit start
	
	if (editType == true) {
		loadPalette("condo/condo.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
		loadPattern("condo/condo.nes", 0, 256);			//Table 0 = condo tiles	
		mapWidth = condoWidth;
		fileNameCondo = '1';			
		populateEditCondo();
	}
	else {
		loadPalette("story/kitten.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
		loadPattern("story/kitten_t.nes", 0, 256);			//Table 0 = condo tiles			

		//loadPalette("story/rob_12.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
		//loadPattern("story/rob_nice_t.nes", 0, 256);			//Table 0 = condo tiles			
		//loadPattern("story/rob_evil_t.nes", 0, 256);			//Table 0 = condo tiles			
		
		
		//loadPalette("story/reporter2.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
		//loadPattern("story/reporter2.nes", 0, 256);			//Table 0 = condo tiles			
		
		//loadPalette("hallway/hallway.dat");            		//Load palette colors from a YY-CHR file. Can be individually changed later on
		//loadPattern("hallway/hallway.nes", 0, 256);			//Table 0 = condo tiles	
		mapWidth = hallwayWidth;
		fileNameCondo = '9';								//Hallway is floor x, condo 0. Edit story = 9

		if (cutscene == true) {
			
		}
		else {
			populateEditHallway();
		}

	}

	editState = tileDrop;			//State of editor to start

	editEntryFlag = true;			//Entry flag, user must release A before we can draw

	redrawEditWindow();	
	
	editAaction = 0;				//0 = drop tiles 1 = copy, 2 = paste, 3 = drop sprite
	
	tileDropAttribute = 3;	//Start with this one so it's more intuitiuve
	
	editDisableCursor = false;
	displayPause(false);   		//Allow core 2 to draw
	isDrawn = true;		
	
}

void populateEditCondo() {
	
	for (int x = 0 ; x < 8 ; x++) {							//8 screens wide
		
		for (int y = 0 ; y < 13 ; y++) {					//Top 13 rows (bottom is floor)
			
			for (int xx = 0 ; xx < 15 ; xx++) {				//Make it all blank
				condoMap[y][(x * 15) + xx] = ' ' | (3 << 8);
			}
		}
		
		condoMap[12][(x * 15) + 14] = 0xF0;					//Screen edge line marker
		
	}	
		
	for (int x = 0 ; x < mapWidth ; x++) {		//Floor
		condoMap[14][x] = 0x5C | (tileBlocked << 8) | (3 << 8);
	}
	for (int x = 0 ; x < mapWidth ; x++) {		//Platform (blank, one above floor)
		condoMap[13][x] = ' ' | (3 << 8);
	}		
	for (int y = 0 ; y < 13 ; y++) {
		condoMap[y][0] = 0x5B | (tileBlocked << 8)| (3 << 8);						//Left wall
	}	
	for (int y = 0 ; y < 13 ; y++) {
		condoMap[y][mapWidth - 1] = 0x5D | (tileBlocked << 8)| (3 << 8);				//Right wall
	}

	for (int g = 0 ; g < 8 ; g++) {
		condoMap[13 - g][0] = 0x79 | (3 << 8);
	}

	const char tiles[4][9] = {
		{ 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A },
		{ 0x8A, 0x8A, 0x8C, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A },	
		{ 0x8A, 0x8A, 0x8D, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A },		
		{ 0x8A, 0x8A, 0x8A, 0x8B, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A },		
	};	

	for (int x = 0 ; x < 4 ; x++) {		
		for (int g = 0 ; g < 8 ; g++) {
			condoMap[13 - g][x + 1] = tiles[x][g];
		}		
	}

	updateMapFileNameEdit();						//Draw the room number on door
	
	for (int x = 0 ; x < mapWidth ; x++) {		//Platform (blank, one above floor)
		condoMap[13][x] |= (tilePlatform << 8);		//Set platform on all tiles above the floor
	}		
		
}


void populateEditHallway() {

	for (int x = 0 ; x < mapWidth ; x++) {								//Floor
		condoMap[14][x] = 0x9E | (tileBlocked << 8) | (3 << 8);							
		condoMap[13][x] = 0x9C | (tilePlatform << 8) | (3 << 8);								//Platform (trim, one above floor)
		
		for (int y = 0 ; y < 13 ; y++) {
			condoMap[y][x] = 0xB2 | (3 << 8);										//Blank wall
		}		
	}		
	for (int y = 0 ; y < 14 ; y++) {
		condoMap[y][0] = 0xB0 | (tileBlocked << 8) | (3 << 8);						//Left wall
	}	
	for (int y = 0 ; y < 14 ; y++) {
		condoMap[y][mapWidth - 1] = 0xB1 | (tileBlocked << 8) | (3 << 8);				//Right wall
	}

	int xx = 4;							//Shelf things

	for (int x = 0 ; x < 3 ; x++) {		
		drawHallwayCounter(xx);		
		drawHallwayCounter(xx + 12);
		xx += 18;	
	}

	xx = 80;

	for (int x = 0 ; x < 3 ; x++) {		
		drawHallwayCounter(xx);		
		drawHallwayCounter(xx + 12);
		xx += 18;	
	}
	
	populateEditDoors();
	
	updateMapFileNameEdit();
	
	//drawHallwayElevator(64);

	condoMap[9][72] = 0xF9 | (1 << 8);				//Call button
	
	drawHallwayCounterWindow(4);
	drawHallwayCounterWindow(128);

}

void populateEditDoors() {

	for (int x = 0 ; x < 6 ; x++) {

		if (apartmentState[x] & 0x40) {			//Door open bit set? (bud emerging from condo)
			drawHallwayDoorOpen(doorTilePos[x], x + 1);
		}
		else {
			drawHallwayDoorClosed(doorTilePos[x], x + 1);			//Closed door
		}

	}	
	
}


void drawEditMenuPopUp() {

	fillTiles(0, 0, 14, 8, ' ', 0);			//Clear area

	if (cursorBlink & 0x01) {
		drawText(">", 0, editMenuSelectionY, false);		
	}
	
	drawText("COPY TO SLOT", 1, 0, false);
	drawDecimal(currentCopyBuffer, 14, 0);	
	drawText("PASTE + SLOT", 1, 1, false);	
	drawDecimal(currentCopyBuffer, 14, 1);	
			//0123456789ABCDE
	drawText("---------------", 1, 2, false);
	drawText("CHANGE FLOOR", 1, 3, false);
	tileDirect(14, 3, fileNameFloor);
	drawText("CHANGE ROOM", 1, 4, false);
	tileDirect(14, 4, fileNameCondo);
	
	drawText("SAVE FILE", 1, 5, false);
	drawText("LOAD FILE", 1, 6, false);
	drawText("EXIT TO MENU", 1, 7, false);		

	int condoZero = 49;				//Normal select range 1-6
	int condoHigh = 54;
	
	if (editType == false) {		//If editing a hallway, the condo # must be 0
		condoZero = 48;
		condoHigh = 57;				//Storylords
		//condoHigh = 48;
	}

	if (button(left_but)) {
		switch(editMenuSelectionY) {		
			case 0:
			case 1:
				if (--currentCopyBuffer < 0) {
					currentCopyBuffer = 3;			//Roll over
				}						
			break;
			
			case 3:
				if (--fileNameFloor < 49) {			//1-6
					fileNameFloor = 54;					
				}
				updateMapFileNameEdit();
			break;
			
			case 4:
				if (--fileNameCondo < condoZero) {			//1-6
					fileNameCondo = condoHigh;
				}	
				updateMapFileNameEdit();	
			break;
		}
	}
	if (button(right_but)) {
		switch(editMenuSelectionY) {		
			case 0:
			case 1:
				if (++currentCopyBuffer > 3) {
					currentCopyBuffer = 0;			//Roll over
				}					
			break;
			
			case 3:
				if (++fileNameFloor > 54) {			//1-6
					fileNameFloor = 49;
				}
				updateMapFileNameEdit();
			break;
			
			case 4:
				if (++fileNameCondo > condoHigh) {			//1-6
					fileNameCondo = condoZero;
				}
				updateMapFileNameEdit();		
			break;
		}		

	}
	if (button(up_but)) {
		if (--editMenuSelectionY < 0) {
			editMenuSelectionY = 7;			//Roll over
		}
	}
	if (button(down_but)) {
		if (++editMenuSelectionY > 7) {
			editMenuSelectionY = 0;				//Roll over
		}
	}
	
	if (button(A_but)) {
	
		int which = editMenuSelectionY;
		
		if (which > 8) {
			which -= 9;
		}
		
		switch(which) {
		
			case 0:		//Copy
				editAaction = 1;
				copySelectWhich = 1;
				makeMessage("UPPER LEFT?");
				setButtonDebounce(A_but, true, 1);
			break;
			
			case 1:		//Paste
				editAaction = 2;
				copySelectWhich = 1;
				makeMessage("UPPER LEFT?");
				setButtonDebounce(A_but, true, 1);
			break;
			
			case 2:		//Undo
			
			break;

			case 5:		//Save
				saveLevel();
				manualAdebounce = true;
			break;
			
			case 6:		//Load
				loadLevel();
				editWindowX = 0;
				manualAdebounce = true;
			break;				

			case 7:		//Exit to menu
				switchGameTo(titleScreen);
			break;
		
			
		}
			
		editMenuActive = false;
		setButtonDebounce(up_but, false, 0);			//Tile sliding debounce
		setButtonDebounce(down_but, false, 0);
		setButtonDebounce(left_but, false, 0);
		setButtonDebounce(right_but, false, 0);
	
		redrawEditWindow();	

	}
	
}

void updateMapFileName() {				//Call thise in edit/game mode before calling save/load
	
	mapFileName[12] = fileNameFloor;
	mapFileName[14] = fileNameCondo;

	//condoMap[8][2] = 0x80 + (fileNameFloor - 48);		//Update the number on the open door
	//condoMap[8][3] = 0x80 + (fileNameCondo - 48);	
	
	//Serial.print("Game level filename:");
	//Serial.println(mapFileName);

}

void updateMapFileNameEdit() {				//Call thise in edit/game mode before calling save/load
	
	mapFileName[12] = fileNameFloor;
	mapFileName[14] = fileNameCondo;

	if (editType == true) {				//Condo? Change the numbers on the open door on the left
		//Serial.print("CONDO MODE: ");
		condoMap[8][2] = 0x80 + (fileNameFloor - 48);		//Update the number on the open door
		condoMap[8][3] = 0x80 + (fileNameCondo - 48);		
	}
	else {
		//Serial.print("HALLWAY MODE: ");
		currentFloor = fileNameFloor - 48;				//Convert ASCII to a number and change the Floor # on top of the elevator
		
		if (cutscene == false) {
			populateEditDoors();
			drawHallwayElevator(64);					
		}
	}

	//Serial.print("Edit filename:");
	//Serial.println(mapFileName);

}

void saveLevel() {

	saveFile(mapFileName);
	
	//return;

	for (int x = 0 ; x < mapWidth ; x++) {				//Platform (blank, one above floor)
		condoMap[14][x] |= (tileBlocked << 8);			//Set floor as blocked
	}	

	for (int x = 1 ; x < (mapWidth - 1) ; x++) {		//Platform (blank, one above floor)
		condoMap[13][x] |= (tilePlatform << 8);			//Set platform on all tiles above the floor
	}	

	for (int y = 0 ; y < 15 ; y++) {				//Write the map file in horizontal strips from left to right, top to bottom
		
		for (int x = 0 ; x < mapWidth ; x++) {
			
			//Each map tile is 16 bit, file access is byte-aligned
			writeByte(condoMap[y][x] >> 8);		//Write high byte first (big endian)
			writeByte(condoMap[y][x] & 0xFF);	//Write low byte second
		}		
		
	}
	
	//SAVE OTHER METADATA HERE (robots, greenies, etc)

	uint8_t objectCount = 0;

	for (int x = 0 ; x < maxThings ; x++) {		//Count objects (they may not be contiguous)
		if (object[x].active == true) {
			objectCount++;
		}
	}
	
	writeByte(128);								//end of tile data
	writeByte(objectCount);						//Write the count after tile data

	//0= tall robot 1=can robot, 2 = flat robot, 3= dome robot, 4= kitten, 5= greenie (greenies on bud sheet 1)

	for (int x = 0 ; x < maxThings ; x++) {		//Greenie scan (top draw priority)
		if (object[x].active == true) {
		
			if (object[x].category == 0) {		//Gameplay object?		
				if (object[x].type == 5) {		//Greenie? Save first
					saveObject(x);
				}			
			}

		}
	}
	
	for (int x = 0 ; x < maxThings ; x++) {		//Tall Robot scan (next priority)
		if (object[x].active == true) {
		
			if (object[x].category == 0) {		//Gameplay object?		
				if (object[x].type == 0) {		//Tall robot? Save these first (so robots on counters/tables etc appear behind them)
					saveObject(x);
				}			
			}

		}
	}	
	
	for (int x = 0 ; x < maxThings ; x++) {		//Robot scan (next priority)
		if (object[x].active == true) {
		
			if (object[x].category == 0) {		//Gameplay object?		
				if (object[x].type > 0 && object[x].type < 4) {		//All robots other than the tall ones? Save next
					saveObject(x);
				}			
			}

		}
	}		

	for (int x = 0 ; x < maxThings ; x++) {		//Kitten (below robots)
		if (object[x].active == true) {
		
			if (object[x].category == 0) {		//Gameplay object?		
				if (object[x].type == 4) {		//Kitten or greenie? Save first
					saveObject(x);
				}			
			}

		}
	}

	for (int x = 0 ; x < maxThings ; x++) {		//Most other objects (will draw behind kittens, robots and greenies)
		if (object[x].active == true) {
		
			if (object[x].category > 1) {		//Not a gameplay object or bad art?
				saveObject(x);		
			}
		}
	}	
	
	for (int x = 0 ; x < maxThings ; x++) {		//Bad art (lowest priority, on walls)
		if (object[x].active == true) {
		
			if (object[x].category == 1) {		//Bad art?		
				saveObject(x);		
			}
		}
	}		
	

	writeByte(255);								//end of object data
	closeFile();								//Close the file when done (doesn't seem to be working???)
	
	makeMessage("SAVED");
	messageTimer = 30;
	redrawEditWindow();	

	
}

void loadLevel() {
	
	if (loadFile(mapFileName) == false) {	
		makeMessage("FILE NOT FOUND");
		//Serial.println("FILE NOT FOUND");
		messageTimer = 30;
		redrawEditWindow();	
		return;		
	}
	
	//Serial.println("LOADING FILE");
	//File exists, so load it!
	
	clearObjects();		

	uint16_t temp;
	
	for (int y = 0 ; y < 15 ; y++) {				//Read the map file in horizontal strips from left to right, top to bottom
		
		for (int x = 0 ; x < mapWidth ; x++) {
			temp = readByte() << 8;					//Read first byte and put into high byte (big endian)		
			temp |= readByte();						//OR next byte into the low byte			
			condoMap[y][x] = temp;
		}		
		
	}	

	//Serial.print(mapWidth * 15);
	//Serial.println(" TILES LOADED");

	if (readByte() == 128) {				//128 = end of tile data. Old files don't have this so skip objects if not found

		uint8_t objectCount = readByte();			//Get # of objects saved in this file

		highestObjectIndex = objectCount;			//Set this as our scan limit

		//Serial.print(objectCount, DEC);
		//Serial.println(" OBJECTS LOADED");
	
		//ADD KITTEN BUBBLE SORT?
	
		for (int x = 0 ; x < objectCount ; x++) {	//Load that many objects into object RAM. They'll populate from 0 up
			object[x].active = false;			    //Kill any existing object (overwrite)
			loadObject(x);							//Load new data
		}
	}

	if (readByte() == 255) {						//Check end (this should be a checksum)
		closeFile();									
		redrawEditWindow();		
		makeMessage("LOADED");
		//Serial.println("LOAD COMPLETE");
		messageTimer = 30;
		redrawEditWindow();					
	}
	else {
		closeFile();	
		redrawEditWindow();		
		makeMessage("LOAD ERROR");
		//Serial.println("LOAD ERROR");
		messageTimer = 30;
		redrawEditWindow();			
	}

	for (int x = 0 ; x < mapWidth ; x++) {		
		condoMap[15][x] = condoMap[14][x];			//Dupe the bottom row into +1 past bottom of tile map so shake doesn't show black
	}	
	
	if (gameState == levelEdit) {					//If editing, make sure the door has right # on it
		updateMapFileNameEdit();					//Ensure the proper floor-condo # is on the door (so we don't have to in Tiled)
	}

	// Serial.print("Total objects loaded from file = ");
	// Serial.println(highestObjectIndex);

}

void editLogic() {

	cursorBlink++;

	if (button(start_but)) {
		
		if (editMenuActive == false) {
			editMenuActive = true;

			editMenuBaseY = 0;
			editMenuSelectionY = editMenuBaseY;			//Starting menu cursor
			
			setButtonDebounce(up_but, true, 1);			//Debounce unless scrolling map left or right
			setButtonDebounce(down_but, true, 1);
			setButtonDebounce(left_but, true, 1);
			setButtonDebounce(right_but, true, 1);	

			
		}
		else {
			editMenuActive = false;
			setButtonDebounce(up_but, false, 0);			//Tile sliding debounce
			setButtonDebounce(down_but, false, 0);
			setButtonDebounce(left_but, false, 0);
			setButtonDebounce(right_but, false, 0);	
			redrawEditWindow();
		}	
		
	}

	if (editMenuActive == true) {
		drawEditMenuPopUp();
		return;		
	}


	bool windowActive = false;

	if (button(B_but)) {						//Selecting tiles or sprites?

		if (bButtonFlag == false) {						//Just switched to menus? Change debounces		
			setButtonDebounce(up_but, true, 1);			//Debounce unless scrolling map left or right
			setButtonDebounce(down_but, true, 1);
			setButtonDebounce(left_but, true, 1);
			setButtonDebounce(right_but, true, 1);	
			setButtonDebounce(A_but, true, 1);			//A is debounced using auto system in menus
			bButtonFlag = true;

		}
		
		windowActive = true;
		
		switch(cursorDrops) {							//Draw whatever menu
		
			case 0:
				windowTileSelect();
			break;
			
			case 1:
				windowTileAttribute();
			break;
			
			case 2:
				windowObjectSelect();
			break;			
		
		}
	
		cursorMenuShow = true;				//Flag to redraw BG when B is released

	}
	else {										//Dropping things
		
		if (bButtonFlag == true) {						//We were in the B menu?	
			setButtonDebounce(up_but, false, 0);			//Tile sliding debounce
			setButtonDebounce(down_but, false, 0);
			setButtonDebounce(left_but, false, 0);
			setButtonDebounce(right_but, false, 0);			
			bButtonFlag = false;
			clearMessage();
			
			if (cursorDrops == 2) {						//Dropping objects, enable debounce so they don't pile up
				editDisableCursor = false;
				editAaction = 10;
				setButtonDebounce(A_but, true, 0);
				makeMessage("PLACE OBJECT");
				messageTimer = 20;						
			}
			else {
				setButtonDebounce(A_but, false, 0);
			}
		}		
			
		if (cursorMenuShow == true) {				//B released and now we're back here? Redraw window
			cursorMenuShow = false;
			redrawEditWindow();
		}	
		bool noMove = true;
			
		if (button(left_but)) {	
			cursorMoveTimer++;			
			if (cursorMoveTimer == 1 || cursorMoveTimer > 15) {		//Did it just start, or was held a while? Allow movement
				
				if (drawFine == true) {
					if (drawXfine > 0) {			//Window doesn't scroll in fine mode
						drawXfine -= 1;
					}				
				}
				else {
					if (drawX > 0) {
						drawX -= 1;
					}
					else {
						if (editWindowX > 0) {
							editWindowX--;	
							redrawEditWindow();
						}
					}	
					drawXfine = drawX << 3;
				}

				if (cursorMoveTimer > 100)	{
					cursorMoveTimer = 100;
				}
			}
			noMove = false;
		}
		if (button(right_but)) {			
			cursorMoveTimer++;		
			if (cursorMoveTimer == 1 || cursorMoveTimer > 15) {		//Did it just start, or was held a while? Allow movement
					
				if (drawFine == true) {
					if (drawXfine < 112) {			//Window doesn't scroll in fine mode
						drawXfine += 1;
					}				
				}
				else {
					if (drawX < 14) {
						drawX += 1;
					}
					else {
						if (editWindowX < (mapWidth - 15)) {
							editWindowX++;	
							redrawEditWindow();
						}
					}	
					drawXfine = drawX << 3;
				}			

				if (cursorMoveTimer > 100)	{
					cursorMoveTimer = 100;
				}
			}
			noMove = false;
		}
		if (button(up_but)) {
			cursorMoveTimer++;			
			if (cursorMoveTimer == 1 || cursorMoveTimer > 15) {		//Did it just start, or was held a while? Allow movement
			
				if (drawFine == true) {
					if (drawYfine > 0) {			//Window doesn't scroll in fine mode
						drawYfine -= 1;
					}				
				}
				else {					
					if (drawY > 0) {
						drawY -= 1;
					}
					drawYfine = drawY << 3;
				}
				if (cursorMoveTimer > 100)	{
					cursorMoveTimer = 100;
				}				
			}
			noMove = false;
		}
		if (button(down_but)) {
			cursorMoveTimer++;			
			if (cursorMoveTimer == 1 || cursorMoveTimer > 15) {		//Did it just start, or was held a while? Allow movement
			
				if (drawFine == true) {
					if (drawYfine < 112) {			//Window doesn't scroll in fine mode
						drawYfine += 1;
					}				
				}
				else {					
					if (drawY < 14) {
						drawY += 1;
					}
					drawYfine = drawY << 3;
				}			

				if (cursorMoveTimer > 100)	{
					cursorMoveTimer = 100;
				}
			}
			noMove = false;
		}	
	
		if (noMove == true) {			//No movement on dpad? Reset timer
			cursorMoveTimer = 0;
		}
		
		if (button(C_but)) {						//Cycle through drop types
			if (++cursorDrops > 2) {
				cursorDrops = 0;
			}
			
			drawFine = false;
			
			switch(cursorDrops) {
				case 0:
					editAaction = 0;
					makeMessage("TILE PLACE");
					messageTimer = 20;
					redrawEditWindow();	
					setButtonDebounce(A_but, false, 0);
				break;
				
				case 1:
					editAaction = 6;
					makeMessage("TILE TYPE");
					messageTimer = 20;
					redrawEditWindow();	
					switch(tileDropAttribute) {		
						case 2:
							tileDropAttributeMask = 0x00;			//free
						break;
						
						case 3:
							tileDropAttributeMask = 0x80;			//platform
						break;
						
						case 4:
							tileDropAttributeMask = 0x40;			//blocked
						break;
					}										
					setButtonDebounce(A_but, false, 0);	
				break;
				
				case 2:
					editAaction = 10;
					makeMessage("OBJECT PLACE");
					messageTimer = 20;
					redrawEditWindow();	
					drawObjectLoadBrush(false);
					setButtonDebounce(A_but, true, 1);					
				break;				
			}
		}			

	}

	if (manualAdebounce == true) {				//Flag that A must be released before it can be pressed again?
		if (button(A_but) == false) {			//Is button open?
			manualAdebounce = false;			//Clear flag
		}		
	}


	if (button(A_but) && manualAdebounce == false && windowActive == false) {
		
		switch (editAaction) {
			
			case 0:		//Drop tile
				condoMap[drawY][editWindowX + drawX] = nextTileDrop | (tilePlacePalette << 8);		//Drop tile in both the tilemap and the condo map
				redrawEditWindow();	
			break;
			
			case 1:		//Copy
				switch(copySelectWhich) {			
					case 1:
						copyBufferUL[currentCopyBuffer][0] = drawX + editWindowX;
						copyBufferUL[currentCopyBuffer][1] = drawY;
						makeMessage("LOWER RIGHT?");
						redrawEditWindow();
						copySelectWhich = 2;
					break;
					
					case 2:
						copyBufferLR[currentCopyBuffer][0] = drawX + editWindowX;
						copyBufferLR[currentCopyBuffer][1] = drawY;
						copyToBuffer();	
						editAaction	= 0;
						setButtonDebounce(A_but, false, 0);	
						makeMessage("COPIED!");
						messageTimer = 30;
						manualAdebounce = true;
					break;
					
				}
			break;
			
			case 2:		//Paste
				editAaction	= 0;
				pasteFromBuffer();
				setButtonDebounce(A_but, false, 0);	
				makeMessage("PASTED!");
				messageTimer = 30;
				manualAdebounce = true;				
			break;

			case 3:		//Save
			
			break;
			
			case 4:		//Load
			
			break;				

			case 5:		//Exit to menu
				//switchMenuTo(mainMenu);
			break;
			
			case 6:		//Drop tile attributes
				condoMap[drawY][editWindowX + drawX] &= 0x0FFF;					//Mask off top nibble
				condoMap[drawY][editWindowX + drawX] |= (tileDropAttributeMask << 8);	//Mask in attribute nibble
				redrawEditWindow();	
			break;
			
			case 10:
				switch(objectDropCategory) {					
					case 0:		//Game stuff
						placeObjectInMap();					//TEMP BJH - it mostly works?
						
						if (selectRobot < 4) {			  //Evil robot? Prompt to set sentry edges
							editAaction = 11;
							makeMessage("SENTRY LEFT?");
							messageTimer = 20;	
							redrawEditWindow();
						}
						else {
							editAaction = 10;				//Else just drop it (greenie/kitten)
						}
						
					break;
					
					case 5:		//Eraser
						eraseFlag = true;
					break;
					
					default:
						placeObjectInMap();
					break;		
				}
			break;
			
			
			case 11:					//Set sentry left edge
				sentryLeft = (editWindowX << 3) + (drawX << 3);			//Get world pixel
				if (sentryLeft >= object[objectIndexLast].xPos) {		//Sentry left has to be at least 1 tile to the left of where you dropped robot		
					makeMessage("INVALID LEFT");
					messageTimer = 20;	
					redrawEditWindow();					
					break;
				}								
				makeMessage("SENTRY RIGHT?");
				messageTimer = 20;	
				redrawEditWindow();		
				editAaction = 12;
			break;
			
			case 12:					//Set sentry right edge
				sentryRight = (editWindowX << 3) + (drawX << 3) + 8;	//The right edge of the recticle is the edge (+8)
				if (sentryRight < object[objectIndexLast].xPos + ((object[objectIndexLast].width + 1) << 3)) {			//Sentry area must be AT LEAST width of robot + 1 tile		
					makeMessage("INVALID RIGHT");
					messageTimer = 20;	
					redrawEditWindow();					
					break;
				}				
				makeMessage("STORED");
				messageTimer = 20;	
				redrawEditWindow();	
				editAaction = 10;
				object[objectIndexLast].xSentryLeft = sentryLeft;
				object[objectIndexLast].xSentryRight = sentryRight;
			break;
		
				
		}

	}
	
	if (editDisableCursor == false) {			//Can cursor be shown?
		
		if (windowActive == true) {
			if (cursorDrops == 0) {				//Dropping tiles?
				drawSprite(drawX << 3, drawY << 3, nextTileDrop, tilePlacePalette, false, false);		//Just draw next tile, no reticle as we can't move anyway. Makes it easier to see how it fits		
			}
			if (cursorDrops == 1) {
				switch(tileDropAttribute) {
					case 3:							
						drawSprite(drawX << 3, drawY << 3, '.', 0, false, false);		//plat
					break;
					
					case 4:							
						drawSprite(drawX << 3, drawY << 3, ',', 0, false, false);		//blocked
					break;
					
					default:
						drawSprite(drawX << 3, drawY << 3, 64, 7, false, false);		//Reticle (free, null)
					break;			
				}				
			}			
		}
		else {
			if (cursorBlink & 0x01) {
				if (drawFine == false) {
					drawSprite(drawX << 3, drawY << 3, 64, 7, false, false);					//Alternate between reticle and object to drop
				}
			}
			else {
				switch(cursorDrops) {
					case 0:
						drawSprite(drawX << 3, drawY << 3, nextTileDrop, tilePlacePalette, false, false);		//Next tile (as sprite)
					break;
					
					case 1:
						switch(tileDropAttribute) {
							case 3:							
								drawSprite(drawX << 3, drawY << 3, '.', 0, false, false);		//plat
							break;
							
							case 4:							
								drawSprite(drawX << 3, drawY << 3, ',', 0, false, false);		//blocked
							break;			
						}						
					break;
					
					case 2:
					
						if (objectDropCategory == 5) {
							drawSprite(drawX << 3, drawY << 3, 0x2C, 3, false, false);							
						}
						else {
							if (drawFine == true) {
								drawSprite(drawXfine, drawYfine, objectSheetIndex[objectDropCategory][0], objectSheetIndex[objectDropCategory][1], objectTypeSize[objectDropCategory][0], objectTypeSize[objectDropCategory][1], objectPlacePalette, objectPlaceDir, false);
							}	
							else {
								drawSprite(drawX << 3, drawY << 3, objectSheetIndex[objectDropCategory][0], objectSheetIndex[objectDropCategory][1], objectTypeSize[objectDropCategory][0], objectTypeSize[objectDropCategory][1], objectPlacePalette, objectPlaceDir, false);							
							}								
						}
						if (button(select_but)) {
							if (drawFine == false) {
								drawFine = true;
								makeMessage("FINE POS");
								messageTimer = 15;
								redrawEditWindow();	
								drawXfine = drawX * 8;
								drawYfine = drawY * 8;
							}
							else {
								drawFine = false;
								makeMessage("COARSE POS");
								messageTimer = 15;
								redrawEditWindow();	
								drawX = drawXfine >> 3;
								drawY = drawYfine >> 3;
							}
						}
					break;	
				}	
			}						
		}
	}

	if (messageTimer > 0) {
		if (--messageTimer == 0) {
			clearMessage();
			redrawEditWindow();			
		}
	}


	if (windowActive == false) {			//Don't draw sprite objects in menu
		editorDrawObjects();
	}
	
}

void placeObjectInMap() {					//Used in edit mode

	int x;

	for (x = 0 ; x < maxThings ; x++) {			//Find first open slot
		if (object[x].active == false) {
			objectIndexNext = x;
			break;
		}				
	}
	
	if (x == maxThings) {			//Didn't find a slot? Abort
		makeMessage("OBJECT LIMIT");
		messageTimer = 40;			
		return;
	}

	object[objectIndexNext].active = true;
	object[objectIndexNext].xPos = (editWindowX << 3) + drawXfine;
	object[objectIndexNext].yPos = drawYfine;
	object[objectIndexNext].sheetX = objectSheetIndex[objectDropCategory][0];
	object[objectIndexNext].sheetY = objectSheetIndex[objectDropCategory][1];
	object[objectIndexNext].width = objectTypeSize[objectDropCategory][0];
	object[objectIndexNext].height = objectTypeSize[objectDropCategory][1];
	object[objectIndexNext].palette = objectPlacePalette;
	object[objectIndexNext].dir = objectPlaceDir;
	
	object[objectIndexNext].xSentryLeft = sentryLeft;
	object[objectIndexNext].xSentryRight = sentryRight;
	
	object[objectIndexNext].category = objectDropCategory;			//0 = gameplay elements >0 BG stuff
	
	if (objectDropCategory == 0) {
		object[objectIndexNext].type = selectRobot;
		if (selectRobot == 5) {			//Greenie?
			object[objectIndexNext].animate = 4;
			object[objectIndexNext].dir = true;
		}
	}
	else {
		object[objectIndexNext].type = objectDropType;
		object[objectIndexNext].animate = 0;		
	}
	
	object[objectIndexNext].subAnimate = 0;
	
	objectIndexLast = objectIndexNext;			//Store the index so when we complete the sentry entry we know where to store it

}

int placeObjectInMap(int xPos, int yPos, int sheetX, int sheetY, int width, int height, int palette, int dir, int category, int type) {	//Used in game (mostly hallway)

	for (int x = 0 ; x < maxThings ; x++) {			//Find first open slot
		if (object[x].active == false) {
			objectIndexNext = x;
			break;
		}				
	}
	
	object[objectIndexNext].active = true;
	object[objectIndexNext].state = 0;				//Default state
	
	object[objectIndexNext].xPos = xPos;
	object[objectIndexNext].yPos = yPos;
	object[objectIndexNext].sheetX = sheetX;
	object[objectIndexNext].sheetY = sheetY;
	object[objectIndexNext].width = width;
	object[objectIndexNext].height = height;
	object[objectIndexNext].palette = palette;
	object[objectIndexNext].dir = dir;

	object[objectIndexNext].category = category;			//0 = gameplay elements >0 BG stuff
	object[objectIndexNext].type = type;
	
	highestObjectIndex++;
	
	// Serial.print("Spawning object #");
	// Serial.print(objectIndexNext);
	// Serial.print(" total objects = ");
	// Serial.println(highestObjectIndex);
	
	return objectIndexNext;

}

void placeObjectInMap(int xPos, int yPos, int sheetX, int sheetY, int width, int height, int palette, int dir, int category, int type, int objectNumber) {	//Used to spawn an item at a forced object # (used for windows in hallway to ensure they appear BEHIND robots)
	
	object[objectNumber].active = true;
	object[objectNumber].state = 0;				//Default state
	
	object[objectNumber].xPos = xPos;
	object[objectNumber].yPos = yPos;
	object[objectNumber].sheetX = sheetX;
	object[objectNumber].sheetY = sheetY;
	object[objectNumber].width = width;
	object[objectNumber].height = height;
	object[objectNumber].palette = palette;
	object[objectNumber].dir = dir;

	object[objectNumber].category = category;			//0 = gameplay elements >0 BG stuff
	object[objectNumber].type = type;
	
}


void testAnimateObjects(bool onOff) {

	fallListClear();

	for (int x = 0 ; x < maxThings ; x++) {			//Scan all possible
		if (object[x].active == true && object[x].category == 0) {		//If object exists and is a robot, turn movement on/off
			object[x].moving = onOff;
		}
	}	
	
}

void clearObjects() {

	for (int x = 0 ; x < maxThings ; x++) {			//Clear all
		object[x].active = false;
		object[x].state = 0;	
		object[x].subAnimate = 0;
		object[x].animate = 0;
	}	
	
	highestObjectIndex = 0;						//Highest index to scan across
	
}

void editorDrawObjects() {
	
	for (int g = 0 ; g < maxThings ; g++) {
		
		if (object[g].active) {
			object[g].scan(editWindowX << 3, 0);			//See if object visible
			
			if (objectDropCategory == 5 && eraseFlag == true) {
				
				if (object[g].hitBox((editWindowX << 3) + (drawX << 3), drawY << 3, (editWindowX << 3) + (drawX << 3) + 8, (drawY << 3) + 8) == true) {	//Does the 8x8 reticle fall within the object bounds?
					object[g].active = 0;	
					eraseFlag = false;
					//Serial.print("Erasing object #");
					//Serial.println(g);
				}
		
			}
	
		}
		
	}
	
	if (eraseFlag == true) {		
		eraseFlag = false;	
	}
	
}

void windowTileSelect() {
	
	int atX = 0;					//Where to draw the window
	int atY = 0;
	
	if (drawY < 7) {
		atY = 8;
	}
	if (drawX < 7) {
		atX = 8;
	}
	
	drawTileSelectWindow(atX, atY);

	if (cursorBlink & 0x01) {
		drawSprite((atX + 3) << 3, (atY + 3) << 3, 64, 7, false, false);		//Draw reticle on center select tile
	}
	
	//Scroll around the available tiles
	if (button(left_but)) {
		if (--tileSelectX < 0) {
			tileSelectX = 15;			//Roll over
		}
	}
	if (button(right_but)) {
		if (++tileSelectX > 15) {
			tileSelectX = 0;			//Roll over
		}
	}
	if (button(up_but)) {
		if (--tileSelectY < 0) {
			tileSelectY = 15;			//Roll over
		}
	}
	if (button(down_but)) {
		if (++tileSelectY > 15) {
			tileSelectY = 0;			//Roll over
		}
	}

	if (button(C_but)) {						//Cycle through palettes
		if (++tilePlacePalette > 3) {
			tilePlacePalette = 0;
		}
	}
	
	editAaction = 0;
	
}

void drawTileSelectWindow(int startX, int startY) {
	
	int atY = tileSelectY;

	for (int y = startY ; y < (startY + 7) ; y++) {
		
		int atX = tileSelectX;		
		for (int x = startX ; x < (startX + 7) ; x++) {
			
			tileDirect(x, y, atX + (atY * 16) | (tilePlacePalette << 8));	

			if (++atX > 15) {		//Rollover
				atX = 0;
			}
			
		}
		if (++atY > 15) {			//Rollover
			atY = 0;
		}
	}
	
	nextTileDrop = ((tileSelectX + 3) & 0x0F) + (((tileSelectY + 3) & 0x0F) * 16);
	
}

void windowTileAttribute() {
	
	int atX = 0;					//Where to draw the window
	int atY = 0;
	
	if (drawY < 7) {
		atY = 10;
	}
	if (drawX < 7) {
		atX = 10;
	}
			
	drawTileSelectAttbWindow(atY);

	if (button(up_but)) {
		if (--tileDropAttribute < 2) {
			tileDropAttribute = 4;			//Roll over
		}
		
	}
	if (button(down_but)) {
		if (++tileDropAttribute > 4) {
			tileDropAttribute = 2;			//Roll over
		}
	}
	
	switch(tileDropAttribute) {		
		case 2:
			tileDropAttributeMask = 0x00;			//free
		break;
		
		case 3:
			tileDropAttributeMask = 0x80;			//platform
		break;
		
		case 4:
			tileDropAttributeMask = 0x40;			//blocked
		break;
	}

	editAaction = 6;

}

void drawTileSelectAttbWindow(int startY) {
	
	fillTiles(0, startY, 14, startY + 4, ' ', 0);			//Clear area

	drawText("TILE ATTRIBUTE", 1, startY, false);
	
	drawText("FREE MOVE", 2, startY + 2, false);
	drawText("PLATFORM", 2, startY + 3, false);
	drawText("BLOCKED", 2, startY + 4, false);
	
	if (cursorBlink & 0x01) {
		drawText(">", 0, startY + tileDropAttribute, false);		
	}	
	
}

void windowObjectSelect() {

	drawObjectSelectWindow();

	if (button(left_but)) {		
		switch(objectDropCategory) {
			case 0:
				if (--selectRobot < 0) {
					selectRobot = 5;
				}			
			break;		
			case 6:
				testAnimateState = false;
				testAnimateObjects(testAnimateState);			
			break;
			
			default:	
				if (--objectDropType < objectTypeRange[objectDropCategory][0]) {
					objectDropType = objectTypeRange[objectDropCategory][1];			//Roll over
				}
			break;
		}
	}
	if (button(right_but)) {
		
		switch(objectDropCategory) {
			case 0:
				if (++selectRobot > 5) {
					selectRobot = 0;
				}			
			break;	

			case 6:
				testAnimateState = true;
				testAnimateObjects(testAnimateState);			
			break;
			
			default:	
				if (++objectDropType > objectTypeRange[objectDropCategory][1]) {
					objectDropType = objectTypeRange[objectDropCategory][0];			//Roll over
				}
			break;
		}		

	}
	
	//0= gameplay (enemies and greenies), 1 = bad art 4x2, 2 = small 2x2, 3= tall 2x3, 4= gizmos 3x2 5=erase
	if (button(up_but)) {
		if (--objectDropCategory < 0) {
			objectDropCategory = 6;			//Roll over
		}
		objectDropType = 0;
	}
	if (button(down_but)) {
		if (++objectDropCategory > 6) {
			objectDropCategory = 0;			//Roll over
		}
		objectDropType = 0;
	}

	if (button(C_but)) {						//Cycle through 8 palettes
		if (++objectPlacePalette  > 7) {
			objectPlacePalette  = 0;
		}
	}	
	
	if (button(A_but)) {						//Flip left/right
		objectPlaceDir = !objectPlaceDir;
	}	
	
	editAaction = 10;							//Disable main edit A	
	editDisableCursor = true;
	
}

void drawObjectSelectWindow() {
	
	fillTiles(0, 0, 14, 8, ' ', 0);							//Clear area
	
       //0123456789ABCDE
	drawText(" ) TYPE *", 5, 1, false);
	
// int objectDropCategory = 1;				//0= gameplay (enemies and greenies), 1 = bad art 4x2, 2 = small 2x2, 3= tall 2x3, 4= appliance 3x2, 5= eraser
// int objectDropType= 0;					//index of object within its category

//0= tall robot 1=can robot, 2 = flat robot, 3= dome robot, 4= kitten, 5= greenie (greenies on bud sheet 1)
//int robotSheetIndex[5][2] = { {0, 0}, {0, 8}, {0, 12}, {8, 0}, {6, 3} };
//int robotTypeSize[5][2] = { {2, 6}, {3, 3}, { 4, 2 }, { 4, 3 }, { 2, 2 } };		//Size index

	drawObjectLoadBrush(true);		//Draw text and show item

	drawText("<  ITEM  >", 5, 4, false);
	
	drawText("A=FLIP H", 5, 6, false);		//Starting dir of robots, no effect on greenes
	drawText("C=COLOR", 5, 7, false);		//No effect on greenies because, duh, they're GREEN ONLY
		    //0123456789ABCDE	
	drawText("SEL=FINE/COARSE", 0, 8, false);
	
}

void drawObjectLoadBrush(bool inMenu) {
	
	switch(objectDropCategory) {
		       //0123456789ABCDE
		case 0:
			objectSheetIndex[objectDropCategory][0] = robotSheetIndex[selectRobot][0];			//Get sheet XY and offset by page 3
			
			if (selectRobot == 5) {			//Greenie?
				objectSheetIndex[objectDropCategory][1] = robotSheetIndex[selectRobot][1] + 16;	
				objectPlacePalette = 5;		//Always green
			}
			else {
				objectSheetIndex[objectDropCategory][1] = robotSheetIndex[selectRobot][1] + 48;	
			}
						
			objectTypeSize[objectDropCategory][0] = robotTypeSize[selectRobot][0];
			objectTypeSize[objectDropCategory][1] = robotTypeSize[selectRobot][1];
			if (inMenu) {
				drawText("GAME PLAY", 5, 2, false);	
				drawSprite(0, 0, objectSheetIndex[objectDropCategory][0], objectSheetIndex[objectDropCategory][1], objectTypeSize[objectDropCategory][0], objectTypeSize[objectDropCategory][1], objectPlacePalette, objectPlaceDir, false);	
			}			
		break;			   
		case 1:						
			objectSheetIndex[objectDropCategory][0] = ((objectDropType & 0x01) << 2);
			objectSheetIndex[objectDropCategory][1] = ((objectDropType >> 1) << 1) + 32;
			if (inMenu) {
				drawText("BAD ART", 5, 2, false);		//type 0-7	
				drawSprite(0, 0, objectSheetIndex[objectDropCategory][0], objectSheetIndex[objectDropCategory][1], objectTypeSize[objectDropCategory][0], objectTypeSize[objectDropCategory][1], objectPlacePalette, objectPlaceDir, false);				
			}
			
		break;
		case 2:
			objectSheetIndex[objectDropCategory][0] = ((objectDropType & 0x03) << 1) + 8;
			objectSheetIndex[objectDropCategory][1] = ((objectDropType >> 2) << 1) + 32;
			if (inMenu) {
				drawText("KITCHEN", 5, 2, false);		//type 0-15
				drawSprite(0, 0, objectSheetIndex[objectDropCategory][0], objectSheetIndex[objectDropCategory][1], objectTypeSize[objectDropCategory][0], objectTypeSize[objectDropCategory][1], objectPlacePalette, objectPlaceDir, false);	
			}						
		break;		
		case 3:			
			objectSheetIndex[objectDropCategory][0] = ((objectDropType & 0x03) << 1) + 8;
			objectSheetIndex[objectDropCategory][1] = 32 + 8;
			if (inMenu) {
				drawText("TALL STUFF", 5, 2, false);	//type 0-3	
				drawSprite(0, 0, objectSheetIndex[objectDropCategory][0], objectSheetIndex[objectDropCategory][1], objectTypeSize[objectDropCategory][0], objectTypeSize[objectDropCategory][1], objectPlacePalette, objectPlaceDir, false);								
			}			
		break;		
		case 4:			
			objectSheetIndex[objectDropCategory][0] = ((objectDropType & 0x01) << 2) + 8;
			objectSheetIndex[objectDropCategory][1] = 32 + 11 + ((objectDropType >> 1) << 1);
			if (inMenu) {
				drawText("APPLIANCE", 5, 2, false);		//type 0-3	
				drawSprite(0, 0, objectSheetIndex[objectDropCategory][0], objectSheetIndex[objectDropCategory][1], objectTypeSize[objectDropCategory][0], objectTypeSize[objectDropCategory][1], objectPlacePalette, objectPlaceDir, false);								
			}			
		break;	
		case 5:		
			if (inMenu) {
				drawText("ERASE", 5, 2, false);		//type 0-3	
				drawSprite(16, 16, 0x2C, 3, false, false);
			}			
		break;	
		case 6:
			if (inMenu) {
				drawText("  TEST MOTION", 0, 2, false);		//type 0-3	
				
				drawText("L<OFF", 0, 1, false);		//type 0-3
				drawText("R>ON", 0, 3, false);		//type 0-3				
			}					
		break;
		
	}
	
	
	
}



void copyToBuffer() {

	int offset = currentCopyBuffer * 15;		//Buffer is 4 screens wide (15 x 4)
	int bufferY = 0;								//Copy whatever starting to the upper left corner of the buffer

	copyBufferSize[currentCopyBuffer][0] = copyBufferLR[currentCopyBuffer][0] - copyBufferUL[currentCopyBuffer][0];
	copyBufferSize[currentCopyBuffer][1] = copyBufferLR[currentCopyBuffer][1] - copyBufferUL[currentCopyBuffer][1];

	for (int y = copyBufferUL[currentCopyBuffer][1] ; y < (copyBufferLR[currentCopyBuffer][1] + 1) ; y++) {	

		int offsetX = offset;
	
		for (int x = copyBufferUL[currentCopyBuffer][0] ; x < (copyBufferLR[currentCopyBuffer][0] + 1) ; x++) {			
			copyBuffer[bufferY][offsetX++] = condoMap[y][x];			
		}	
		bufferY++;
	}
		
}

void pasteFromBuffer() {
	
	int offset = currentCopyBuffer * 15;		//Buffer is 4 screens wide (15 x 4) start at the left edge of the specified buffer

	int bufferY2K = drawY;

	for (int y = 0 ; y < (copyBufferSize[currentCopyBuffer][1] + 1) ; y++) {	

		for (int x = 0 ; x < (copyBufferSize[currentCopyBuffer][0] + 1) ; x++) {			
			condoMap[bufferY2K][editWindowX + drawX + x] = copyBuffer[y][x + offset];			
		}	
		bufferY2K++;		
	}

	redrawEditWindow();
	
}

void redrawEditWindow() {

	switch(cursorDrops) {
	
		case 0:
		case 2:
			for (int y = 0 ; y < 15 ; y++) {							//Draw by row		
				for (int xx = 0 ; xx < 15 ; xx++) {						//Draw across
					tileDirect(xx, y, condoMap[y][editWindowX + xx]);
				}
			}
		break;
		
		case 1:
			for (int y = 0 ; y < 15 ; y++) {							//Draw by row		
				for (int xx = 0 ; xx < 15 ; xx++) {						//Draw across
				
					uint16_t temp = condoMap[y][editWindowX + xx];			//Get the tile from map

					switch(temp & 0xF000) {
						case 0x8000:							//plat
							tileDirect(xx, y, '.');
						break;
						
						case 0x4000:							//blocked
							tileDirect(xx, y, ',');
						break;
						
						default:
							tileDirect(xx, y, temp);			//Send as standard (free)
						break;			
					}

				}
			}
		break;
	
	}
	
	if (message[0] != 0) {		
		drawText(message, 0, 14);
	}

}

void saveObject(int which) {

	writeBool(object[which].active);
	writeBool(object[which].visible);
	writeByte(object[which].state);
	writeByte(object[which].whenBudTouch);
	writeByte(object[which].xPos >> 8);
	writeByte(object[which].xPos & 0x00FF);
	writeByte(object[which].yPos >> 8);
	writeByte(object[which].yPos & 0x00FF);

	writeBool(object[which].singleTile);
	writeByte(object[which].sheetX);	
	writeByte(object[which].sheetY);
	writeByte(object[which].width);
	writeByte(object[which].height);
	writeByte(object[which].palette);
	writeByte(object[which].category);	//0= gameplay (enemies and greenies), 1 = bad art 4x2, 2 = small 2x2, 3= tall 2x3, 4= appliance 3x2, 5= eraser
	writeByte(object[which].type);		//What kind of object this is within category
	
	writeBool(object[which].dir);		
	
	writeBool(object[which].turning);
	writeByte(object[which].animate);	
	writeByte(object[which].subAnimate);	

	writeByte(object[which].xSentryLeft >> 8);
	writeByte(object[which].xSentryLeft & 0x00FF);	
	writeByte(object[which].xSentryRight >> 8);	
	writeByte(object[which].xSentryRight & 0x0FF);	
	
	writeBool(object[which].moving);					
	writeBool(object[which].extraY);
			
	writeByte(object[which].stunTimer);	
	writeByte(object[which].speedPixels);	
	writeByte(object[which].extraC);	
	writeByte(object[which].extraD);	
	
	
}

void loadObject(int which) {

	object[which].active = readBool();
	object[which].visible = readBool();
	object[which].state = readByte();
	object[which].whenBudTouch = readByte();
	object[which].xPos = readByte() << 8;
	object[which].xPos |= readByte();
	object[which].yPos = readByte() << 8;
	object[which].yPos |= readByte();

	object[which].singleTile = readBool();
	object[which].sheetX = readByte();	
	object[which].sheetY = readByte();
	object[which].width = readByte();
	object[which].height = readByte();	
	object[which].palette = readByte();	
	object[which].category = readByte();	
	object[which].type = readByte();	
	
	object[which].dir = readBool();		
	
	object[which].turning = readBool();
	object[which].animate = readByte();		
	object[which].subAnimate = readByte();		
	
	object[which].xSentryLeft = readByte() << 8;
	object[which].xSentryLeft |= readByte();
	object[which].xSentryRight = readByte() << 8;
	object[which].xSentryRight |= readByte();	
	
	object[which].moving = readBool();				
	object[which].extraY = readBool();
			
	object[which].stunTimer = readByte();		
	object[which].speedPixels = readByte();		
	object[which].extraC = readByte();		
	object[which].extraD = readByte();		
	
}

void makeMessage(const char *text) {

	for (int x = 0 ; x < 15 ; x++) {
		message[x] = 0;
	}
	
	int x = 0;
	
	while(*text) {
		message[x++] = *text++;			//Just draw the characters from left to write
	}
	
}	

void clearMessage() {
	
	for (int x = 0 ; x < 15 ; x++) {
		message[x] = 0;
	}

	redrawEditWindow();

}	


//Game callbacks------------------------------------------------------------

bool wavegen_callback(struct repeating_timer *t) {
    music.ProcessWaveforms();
    return true;
}

bool sixtyHz_callback(struct repeating_timer *t) {
    music.ServiceTracks();	
	nextFrameFlag += 1;								//When main loop sees this reach 2, run 30Hz game logic	
    return true;
}