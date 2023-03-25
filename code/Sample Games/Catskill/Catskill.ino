#include <gameBadgePico.h>
//#include "thingObject.h"
#include "gameObject.h"
//Your defines here------------------------------------
//#include "hardware/pwm.h"

struct repeating_timer timer30Hz;			//This runs the game clock at 30Hz

char testX = 0;

bool firstFrame = false;
bool displayPause = true;

bool paused = true;							//Player pause, also you can set pause=true to pause the display while loading files/between levels
bool nextFrameFlag = false;					//The 30Hz IRS sets this flag, Core0 Loop responds to it
bool drawFrameFlag = false;					//When Core0 Loop responses to nextFrameFlag, it sets this flag so Core1 will know to render a frame
bool frameDrawing = false;					//Set TRUE when Core1 is drawing the display. Core0 should wait for this to clear before accessing video memory

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

uint16_t budWorldX = 8 * (6 + 7);			//Bud's position in the world (not the same as screen or tilemap)
uint16_t budWorldY = 8;

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

uint16_t worldX = 6 * 8;					//Where the camera is positioned in the world (not the same as tilemap as that's dynamic)
uint16_t worldY = 0;

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

enum stateMachine { bootingMenu, splashScreen };		//State machine of the badge (boot, wifi etc)
stateMachine badgeState = bootingMenu;

enum stateMachineGame { titleScreen, levelEdit, saveMap, loadMap, game, story, goHallway, goCondo, goJail, goElevator, pauseMode};					//State machine of the game (under stateMachine = game)
stateMachineGame gameState = titleScreen;

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

int kittenTotal = 0;					//# of kittens per level
int kittenCount = 0;					//How many player has recused per level
int kittenCountLevel = 0;				//How many kittens per level (inc'd on condo exit)
int kittenMessageTimer = 0;

int budDeath = 0;						//If set, move Bud to center of screen then switch to KITTEN JAIL!!!

int jailYpos;
int budLives;
int budPower;
int budBlink;
int budPalette;				//Uber hack to make Bud blink when invincible, if blink & 1 budPalette = 8 which is blank palette RAM
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
long propDamage = 0;
int robotKill = 0;

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

//Master loops
void setup() { //------------------------Core0 handles the file system and game logic

	gamebadge3init();						//Init system

	menuTimer = 0;

	add_repeating_timer_ms(-33, timer_isr, NULL, &timer30Hz);		//Start 30Hz interrupt timer. This drives the whole system

}

void setup1() { //-----------------------Core 1 builds the tile display and sends it to the LCD
  
}

void loop() {	//-----------------------Core 0 handles the main logic loop
	
	if (nextFrameFlag == true) {		//Flag from the 30Hz ISR?
		nextFrameFlag = false;			//Clear that flag		
		drawFrameFlag = true;			//and set the flag that tells Core1 to draw the LCD

		if (gameActive == true) {
			gameFrame();				//Now do our game logic in Core0			
		}
		else {
			menuFrame();				//Or menu logic
		}
			
		serviceDebounce();				//Debounce buttons
	}

	if (displayPause == true) {  		//If paused for USB xfer, push START to unpause 
		if (button(start_but)) {        //Button pressed?
		  displayPause = false;
		  Serial.println("LCD DMA UNPAUSED");
		  switchGameTo(titleScreen);
		}    
	}

  
}

void loop1() { //------------------------Core 1 handles the graphics blitter. Core 0 can process non-graphics game logic while a screen is being drawn

	if (drawFrameFlag == true) {			//Flag set to draw display?

		drawFrameFlag = false;				//Clear flag
		
		if (displayPause == true) {			//If display is paused, don't draw (return)
			return;
		}		

		frameDrawing = true;				//Set flag that we're drawing a frame. Core0 should avoid accessing video memory if this flag is TRUE
		sendFrame();						//Render the sprite-tile frame and send to LCD via DMA
		frameDrawing = false;				//Clear flag. Core0 is now free to access screen memory (I guess this core could as well?)

	}

}

bool timer_isr(struct repeating_timer *t) {				//Every 33ms, set the flag that it's time to draw a new frame (it's possible system might miss one but that's OK)
	nextFrameFlag = true;
	return true;
}


//Splash screens, boot and menus-----------------------------
void menuFrame() {		//This is called 30 times a second. It either calls the main top menu state machine or the game state machine

	if (displayPause == false) {				//If the display is being actively refreshed we need to wait before accessing video RAM
		
		while(frameDrawing == false) {			//Wait for Core1 to begin rendering the frame
			delayMicroseconds(1);				//Do almost nothing (arduino doesn't like empty while()s)
		}
		//OK we now know that Core1 has started drawing a frame. We can now do any game logic that doesn't involve accessing video memory
	
		serviceAudio();
		
		//Controls, file access, Wifi etc...
		
		while(frameDrawing == true) {			//OK we're done with our non-video logic, so now we wait for Core 1 to finish drawing
			delayMicroseconds(1);
		}
		//OK now we can access video memory. We have about 15ms in which to do this before the next frame starts--------------------------
	}

	switch(badgeState) {
	
		case bootingMenu:							//Special type of load that handles files not being present yet
			if (menuTimer == 65534) {				//Let one frame be drawn to show the LOAD USB FILES message then...
				displayPause = true;				//Spend most of the time NOT drawing the screen so files can load easier
			}

			if (menuTimer > 0) {					//If timer active, decrement
				--menuTimer;				
			}
			
			if (menuTimer == 0) {					//First boot, or did timer reach 0?
				if (loadRGB("UI/NEStari.pal")) {				//Try and load master pallete from root dir (only need to load this once)
					//Load success? Load the rest
					loadPalette("UI/basePalette.dat");          //Load palette colors from a YY-CHR file. Can be individually changed later on
					loadPattern("UI/logofont.nes", 0, 256);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)					
					switchMenuTo(splashScreen);					//Progress to splashscreen					
				}
				else {								//No files? Make USB file load prompt using flash text patterns and wait for user reset
					displayPause = false;   		//Just draw this one
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
		
	}
	
	if (soundTimer > 0) {
		if (--soundTimer == 0) {
			pwm_set_freq_duty(6, 0, 0);
		}
	}

}

void switchMenuTo(stateMachine x) {		//Switches state of menu

	gameActive = false;
	displayPause = true;
	badgeState = x;		
	isDrawn = false;
	
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
	displayPause = false;   		//Allow core 2 to draw
	isDrawn = true;
	
	playAudio("audio/gameBadge.wav");		//Splash audio
	
}

void drawMainMenu() {

	loadPalette("UI/basePalette.dat");          //Load palette colors from a YY-CHR file. Can be individually changed later on
	loadPattern("UI/menuFont.nes", 0, 256);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)

	fillTiles(0, 0, 14, 14, ' ', 3);			//CLS
	
	drawText("MAIN MENU", 3, 0, false);

	drawText("Run GAME", 2, 4, false);
	drawText("Run GAME", 2, 5, false);
	drawText("USB load", 2, 6, false);
	
	drawText("A=SELECT", 3, 14, false);

	cursorX = 1;
	cursorY = 4;
	cursorAnimate = 0x02;
	cursorTimer = 0;

	setWindow(0, 0);

	setButtonDebounce(up_but, true, 1);			//Set debounce on d-pad for menu select
	setButtonDebounce(down_but, true, 1);
	setButtonDebounce(left_but, true, 1);
	setButtonDebounce(right_but, true, 1);
	
	displayPause = false;   		//Allow core 2 to draw
	isDrawn = true;
	
}

void gameFrame() {
	
	if (displayPause == false) {				//If the display is being actively refreshed we need to wait before accessing video RAM
		
		while(frameDrawing == false) {			//Wait for Core1 to begin rendering the frame
			delayMicroseconds(1);				//Do almost nothing (arduino doesn't like empty while()s)
		}
		//OK we now know that Core1 has started drawing a frame. We can now do any game logic that doesn't involve accessing video memory
	
		serviceAudio();
		
		//Controls, file access, Wifi etc...
		
		while(frameDrawing == true) {			//OK we're done with our non-video logic, so now we wait for Core 1 to finish drawing
			delayMicroseconds(1);
		}
		//OK now we can access video memory. We have about 15ms in which to do this before the next frame starts--------------------------
	}

	switch(gameState) {
				
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
			
			fillTiles(cursorX, 9, cursorX, 12, '@', 0);					//Erase arrows
			
			drawTile(cursorX, cursorY, cursorAnimate, 0, 0);			//Animated arrow

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

			tileDirect(cursorX, 8, (((currentFloor - 1) * 16) + 128) + 15);

			if (button(up_but)) {
				if (cursorY > 9) {
					cursorY--;
					pwm_set_freq_duty(6, 493, 25);
					soundTimer = 10;
				}				
			}
			if (button(down_but)) {
				if (cursorY < 12) {
					cursorY++;
					pwm_set_freq_duty(6, 520, 25);
					soundTimer = 10;
				}				
			}
			
			if (button(A_but)) {
				menuTimer = 0;
				switch(cursorY) {					
					case 9:
						switchGameTo(game);
					break;
					
					case 10:
						whichScene = 0;
						switchGameTo(story);
					break;
					
					case 11:
						editType = true;			//Press A to edit condo
						switchGameTo(levelEdit);
					break;
					
					case 12:
						switchGameTo(pauseMode);				
					break;				
				}		
			}
			if (button(B_but) && cursorY == 11) {
				menuTimer = 0;				
				editType = false;					//Press B to edit hallway
				cutscene = true;					//Don't pre-draw doors or anything
				switchGameTo(levelEdit);	
			}
			//TIMER TO ATTRACT
			//SELECT START
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
			startGame();			
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
	
			
		case pauseMode:
			if (isDrawn == false) {				//Draw menu...
				drawPauseMenu();
			}
			else {
				displayPause = true;			//and then pause on next frame
			}	
			break;	
						
		break;
		
		
	}
	
	if (soundTimer > 0) {
		if (--soundTimer == 0) {
			pwm_set_freq_duty(6, 0, 0);
		}
	}

	
}

void switchGameTo(stateMachineGame x) {		//Switches state of game

	gameActive = true;
	displayPause = true;
	gameState = x;		
	isDrawn = false;
	
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


	drawText("start", 10, 9, false);
	drawText("load", 10, 10, false);	
	drawText("edit", 10, 11, false);		
	drawText("usb", 10, 12, false);	

	setWindow(0, 0);
	
	setButtonDebounce(up_but, true, 1);			//Set debounce on d-pad for menu select
	setButtonDebounce(down_but, true, 1);
	setButtonDebounce(left_but, true, 1);
	setButtonDebounce(right_but, true, 1);	
	
	setButtonDebounce(A_but, true, 1);
	setButtonDebounce(B_but, true, 1);
	setButtonDebounce(C_but, true, 1);
	
	menuTimer = random(50, 150);
	cursorTimer = 0;
	cursorX = 9;
	cursorY = 9;
	cursorAnimate = 0x8A;
					
	isDrawn = true;	
	displayPause = false;
	
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
	
	Serial.println("LCD DMA PAUSED");
	
	isDrawn = true;
	displayPause = false;   		//Allow core 2 to draw
	
}


//Gameplay-----------------------------------------------

void startGame() {

	propDamage = 0;
	robotKill = 0;

	//currentFloor = 1;			//Setup game here
	currentCondo = 0;			//Bud hasn't entered a condo
	budLives = 9;
	budPower = 3;
	hallwayBudSpawn = 0;		//Bud jumps through glass

	budPower = 3;
	budStunned = 0;

	elevatorOpen = false;		//Level starting (if die in hallway, spawn in front of closed elevator)
	
	stopWatchClear();
	
	for (int x = 0 ; x < 6 ; x++) {
		apartmentState[x] = 0;
	}
	
	breakWindow = true;
	
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

	fillTiles(0, 0, 31, 15, ' ', 0);			//Black BG
			
	setWindow(0, 0);
	menuTimer = 60;				//Jail for 4 seconds

	jailYpos = -80;

	displayPause = false;   		//Allow core 2 to draw
	isDrawn = true;		
	
}

void jailLogic() {

	drawSprite(40, jailYpos, 8, 16 + 10, 5, 5, 7, false, false);

	drawSprite(40, 80, 0x01F8, 0, false, false);
	drawSprite(56, 80, 0x01F9, 0, false, false);
	drawSpriteDecimal(budLives, 72, 80, 0);
	
	if (menuTimer > 50) {
		drawSprite(52, 47, 13, 16 + 13, 2, 3, 4, false, false);		//Sitting
	}
	else {
		drawSprite(52, 47, 13, 16 + 10, 2, 3, 4, false, false);		//Paws on bars
	}

	if (jailYpos == 24) {
		playAudio("audio/jaildoor.wav");
		budLives--;
	}

	if (jailYpos < 32) {
		jailYpos += 8;
	}
	else {
		if (--menuTimer == 0) {
			
			if (budLives == 0) {
				//GAME OVER				
			}
			else {
				budPower = 3;
				
				switch(lastMap) {
					
					case condo:
						switchGameTo(goCondo);
					break;
					
					case hallway:
						budPower = 3;
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

	kittenCount = 0;		//Reset rescue
	kittenTotal = 0;		//Reset total found

	for (int x = 0 ; x < maxThings ; x++) {			//Find first open slot
		if (object[x].active == true && object[x].category == 0 && object[x].type == 4) {		//Count kittens
			kittenTotal++;
		}
	}

	isDrawn = true;	
	displayPause = false;
	
	kittenMessageTimer = 90;

	budStunned = 0;
	
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
	
	kittenCountLevel += kittenCount;	//Condo cleared? Add how many kittens we recused to floor total
	
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

	stopWatchTick();

	if (budState != dead) {						//Messages top sprite pri, but don't draw if Bud dying
		if (kittenMessageTimer > 0) {
				
			//0123456789ABCDE
			//   RESCUE XX
			//	  KITTENS
			// GOTO THE EXIT
			
			if (kittenMessageTimer & 0x04) {
				int offset = 4;
				
				if ((kittenTotal - kittenCount) > 9) {
					offset = 0;
				}
		
				if (kittenCount == kittenTotal) {
					drawSpriteText("GOTO THE EXIT", 8, 56, 3);
				}
				else {
					drawSpriteText("RESCUE", 28 + offset, 52, 3);
					drawSpriteDecimal(kittenTotal - kittenCount, 84 + offset, 52, 3);		//Delta of kittens to save
					
					if (kittenCount == (kittenTotal - 1)) {		//Only one KITTENS left? Draw a sloppy slash over the S
						drawSpriteText("      /", 36, 60, 3);	
					}				
					drawSpriteText("KITTENS", 36, 60, 3);								
				}
			}	
			kittenMessageTimer--;
		}

		if (kittenCount == kittenTotal) {
			
			drawSprite(7 - inArrowFrame, 88, 6, 16 + 12, 1, 2, 4, false, false);		//out arrow sprite <		
			drawSprite(16, 84, 7, 16 + 11, 1, 3, 4, false, false);		//OUT text
			
			if (++inArrowFrame > 7) {
				inArrowFrame = 0;
			}		
			
		}
	}

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
	
	setCoarseYRollover(0, 14);   //Sets the vertical range of the tilemap, rolls back to 0 after 29

	switch(hallwayBudSpawn) {
	
		case 0:				//Breaking glass stage 1
			budSpawned = false;
			budSpawnTimer = 0;
			spawnIntoHallway(0, 5, 40);
			break;
					
					
		case 11:				//Spawn in front of elevator (if died in hallway)
			budSpawned = true;
			budDir = false;		
			budFrame = 0;
			budVisible = false;
			budState = rest;
			jump = 0;
			spawnIntoHallway(60, 7, 104);
			break;					
					
		case 10:			//Emerge from elevator (stage 2-6)
			budSpawned = true;
			budDir = false;		
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
	
	levelComplete = false;

	fileNameFloor = '1';		//Hallway hub is floor 1, condo 0
	fileNameCondo = '0';
	updateMapFileName();
	loadLevel();								//Load hallway level tiles, we will populate objects and redraw doors manually

	placeObjectInMap(152, 0, 4, 32 + 8, 4, 3, 7, true, 1, 50);		//Chandeliers
	placeObjectInMap(296, 0, 4, 32 + 8, 4, 3, 7, true, 1, 50);
	placeObjectInMap(440, 0, 4, 32 + 8, 4, 3, 7, true, 1, 50);
	placeObjectInMap(616, 0, 4, 32 + 8, 4, 3, 7, true, 1, 50);	
	placeObjectInMap(760, 0, 4, 32 + 8, 4, 3, 7, true, 1, 50);	
	placeObjectInMap(904, 0, 4, 32 + 8, 4, 3, 7, true, 1, 50);	
				
	entryWindow = placeObjectInMap(32, 48, 4, 32 + 12, 4, 3, 6, true, 10, 5);		//Spawn an intact window and get the index #
	
	if (currentFloor == 1) {						//Broken window only on first floor
		if (currentCondo > 0) {								//Bud has already been in a condo? (not game opening)
			if (currentFloor == 1) {
				object[entryWindow].sheetX = 0;				//Window was already broken
			}
		}				
	}
	//Else leave window intact, as it was spawned
	
	placeObjectInMap(1025, 48, 4, 32 + 12, 4, 3, 6, true, 10, 5);		//Other window
	
	populateEditDoors();			//Redraw stuff that changes (door #'s, door open close, etc)
	drawHallwayElevator(64);

	movingObjects(true);

	redrawMapTiles();					//Reload what's visible

	robotsToSpawn = 4; //currentFloor;	//How many robots spawn in the hallway (same as floor #)
	robotsSpaceTimer = 90;			//Time between robot spawns, in ticks

	displayPause = false;   		//Allow core 2 to draw
	isDrawn = true;	
	
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

	stopWatchTick();

	if (budState != dead) {						//Messages top sprite pri, but don't draw if Bud dying
		if (kittenMessageTimer > 0) {
			if (kittenMessageTimer & 0x04) {
				drawSpriteText("GOTO ELEVATOR", 8, 56, 3);
			}	
			kittenMessageTimer--;
		}
	}

	drawBudStats();

	if (budSpawned == true) {
		budLogic2();
	}
	else {
		if (++budSpawnTimer == 20) {
			budSpawned = true;
			jump = 6;
			velocikitten = 5;

			object[entryWindow].sheetX = 0;			//Break the existing window
			playAudio("audio/glass_2.wav");

			int glass = placeObjectInMap(32, 40, 0, 32 + 8, 4, 3, 6, true, 10, 6);		//Spawn broken glass	
			object[glass].subAnimate = 0;
			object[glass].animate = 0;
			object[glass].moving = true;
		}
	}

	//thingLogic();
	setWindow((xWindowCoarse << 3) | xWindowFine, yPos);			//Set scroll window
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
			
			int robot = placeObjectInMap(x, 64, 0, 48 + 0, 2, 6, 4 + random(0, 4), dir, 0, 0);
			
			object[robot].xSentryLeft = 8;
			object[robot].xSentryRight = 1080;	
			object[robot].moving = true;		//Move robot
			
		}
	}

	objectLogic();

}


//Object scan------------------------------------
void objectLogic() {
	
	xShake = 0;
	yShake = 0;

	for (int g = 0 ; g < highestObjectIndex ; g++) {	//Scan no higher than # of objects loaded into level
		
		if (object[g].active) {
			object[g].scan(worldX + xShake, 0 + yShake);					//Draw object if active, plus logic (in object) Robots move if off-camera

			if (object[g].state == 100) {				//Object hit floor?
				if (object[g].type == 50) {				//Chandelier? Big sound
					playAudio("audio/glass_2.wav");				
				}
				else {
					playAudio("audio/glass_loose.wav");
				}
				fallListRemove(g);						//Remove it from active lists (also kills object)
				object[g].state = 101;					//Null state (rubble on floor, still active object)
			}

			if (object[g].visible) {

				if (object[g].category == 0) {
					if (object[g].type < 4) {				//Robot?
						
						switch(object[g].state) {
						
							case 200:				//Short circuit
							
								break;
								
							case 201:
								playAudio("audio/explode.wav");
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
							playAudio("audio/thankYou.wav");	
							object[g].xPos += 4;			//Center rescuse kitten on sitting kitten
							object[g].yPos -= 4;
							object[g].state = 200;			//Rescue balloon
							object[g].animate = 0;
							kittenMessageTimer = 60;		//Show remain
							kittenCount++;					//Saved count
						}
						
					}	
					
					if (object[g].type == 5 && budPower < 3) {						//Greenie? Only run logic if Bud needs power (no pickup on full power)
						
						if (object[g].hitBoxSmall(budWx1, budWy1, budWx2, budWy2) == true) {	//EAT????
							score += 100;
							object[g].active = 0;
							playAudio("audio/greenie.wav");
							if (++budPower > 3) {
								budPower = 3;
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

				if (budState == swiping) {	//Bud can only swipe inert objects
					
					if (object[g].category > 0 && object[g].state == 0) {	//Bud can only swipe inert objects
						
						if (object[g].hitBox(budAx1, budAy1, budAx2, budAy2) == true) {
							
							propDamage++;							
							
							fallListAdd(g);							//Add this item to the fall list
							
							playAudio("audio/slap.wav");	
							object[g].state = 99;					//Falling
							object[g].animate = 1;
						}
						
					}
					
					if (object[g].category == 0 && object[g].type == 4 && object[g].state == 0) {	//If bud hits a kitten...
						
						if (object[g].xSentryLeft == 0 && object[g].hitBox(budAx1, budAy1, budAx2, budAy2) == true) {
							
							score += 10;						//Score hack!
							
							object[g].xSentryLeft = 40;			//Cooldown for swat sounds
							
							kittenMeow = random(0, 4);
							
							switch(kittenMeow) {
								case 0:
									playAudio("audio/kitmeow0.wav");
								break;
								
								case 1:
									playAudio("audio/kitmeow1.wav");
								break;
								
								case 2:
									playAudio("audio/kitmeow2.wav");
								break;
								
								case 3:
									playAudio("audio/kitmeow3.wav");
								break;
							
							}

						}
						
					}					
					
					
					
					
				}	
				
				if (object[g].category == 1 && jump > 0 && object[g].state == 0) {	//Bud can knock loose bad art by jumping through it
					
					if (object[g].hitBox(budWx1, budWy1, budWx2, budWy2) == true) {		
						propDamage++;
						fallListAdd(g);							//Add this item to the fall list						
						playAudio("audio/slap.wav");	
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

	kittenCountLevel = 30;
	
	robotKill = 20;
	
	propDamage = 10;


	bonusPhase = 1;
	bonusTimer = 0;
	menuTimer = 0;

	seconds = 48;
	minutes = 1;

	isDrawn = true;	
	displayPause = false;

	
}

void elevatorLogic() {

	// if (menuTimer > 0) {
		// if (--menuTimer > 0) {
			// return;
		// }
	// }

	fillTiles(0, 0, 14, 14, ' ', 0);			//Clear area

	switch(bonusPhase) {
	
		case 1:
			drawText(" KITTENS SAVED", 0, 2, false);
			drawDecimal(kittenCountLevel, 6, 3);

			if (++bonusTimer > 1) {
				pwm_set_freq_duty(6, 520, 25);
				bonusTimer = 0;
				if (kittenCountLevel < 1) {
					playAudio("audio/cash.wav");
					menuTimer = 40;
					bonusPhase = 2;
				}
				else {
					kittenCountLevel--;
					levelBonus += 2000;				
				}
			}
			else {
				pwm_set_freq_duty(6, 0, 0);	
			}
		break;	
	
	
		case 2:
			drawText("ROBOTS  SMASHED", 0, 2, false);
			drawDecimal(robotKill, 6, 3);

			if (++bonusTimer > 1) {
				pwm_set_freq_duty(6, 520, 25);
				bonusTimer = 0;
				if (robotKill < 1) {
					playAudio("audio/cash.wav");
					menuTimer = 40;
					bonusPhase = 3;
				}
				else {
					robotKill--;
					levelBonus += 500;					
				}
			}
			else {
				pwm_set_freq_duty(6, 0, 0);	
			}
		break;

		case 3:
			drawText("PROPERTY DAMAGE", 0, 2, false);
			drawDecimal(propDamage, 6, 3);
			
			if (++bonusTimer > 1) {
				pwm_set_freq_duty(6, 520, 25);
				bonusTimer = 0;
				if (propDamage < 1) {
					playAudio("audio/cash.wav");
					menuTimer = 40;
					bonusPhase = 4;
				}	
				else {
					propDamage--;			
					levelBonus += 100;					
				}
			}
			else {
				pwm_set_freq_duty(6, 0, 0);	
			}			
		break;
		
		case 4:
			drawText("  TIME BONUS   ", 0, 2, false);
			drawText("00", 5, 3, false);
			
			if (minutes > 9) {
				drawDecimal(minutes, 5, 3);
			}
			else {
				drawDecimal(minutes, 6, 3);
			}
			
			drawText(":0", 6, 3, false);
			
			if (seconds > 9) {
				drawDecimal(seconds, 8, 3);
			}
			else {
				drawDecimal(seconds, 9, 3);
			}
			
			if (seconds & 1) {
				pwm_set_freq_duty(6, 520, 25);
			}
			else {
				pwm_set_freq_duty(6, 0, 0);
			}

			seconds--;
			levelBonus += 5;
			if (seconds < 0) {
				seconds = 59;
				if (--minutes < 0) {
					playAudio("audio/cash.wav");
					bonusPhase = 5;
					score += levelBonus;
					levelBonus = 0;
					menuTimer = 50;
				}
			}
		break;
		
		case 5:
			pwm_set_freq_duty(6, 0, 0);
			if (--menuTimer < 1) {
				nextFloor();
			}		
		break;		
		
		
	}
	
	drawText("               ", 0, 7, false);
	drawText("  LEVEL BONUS", 0, 6, false);
	drawDecimal(levelBonus, 5, 7);	
	
	drawText("     SCORE", 0, 9, false);
	drawDecimal(score, 5, 10);	
	
	
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
			menuTimer = 130;
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
			break;
		
	}
	
	
	setCoarseYRollover(0, 14);   //Sets the vertical range of the tilemap, rolls back to 0 after 29
	setWindow(0, 0);
	isDrawn = true;	
	displayPause = false;
		
	
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
			
			if (++cutSubAnimate == 20 && explosionCount < 5) {
				cutSubAnimate = 0;
				budX = random(72, 96);
				budY = random(16, 56);
				cutSubAnimate2 = 8;
				playAudio("audio/bomb.wav");
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
			
			if (menuTimer == 119) {
				fillTiles(0, 10, 14, 14, ' ', 3);			//Clear text area, white palette
					        //0123456789ABCDE
					drawText("WE ARE GETTING", 0, 11, false);
					drawText(" REPORTS...", 0, 12, false);
					playAudio("audio/kitmeow0.wav");
			}
			if (menuTimer == 70) {
				fillTiles(0, 10, 14, 14, ' ', 3);			//Clear text area, white palette
					        //0123456789ABCDE
					drawText(" THAT INNOCENT", 0, 11, false);
					drawText("  KITTENS ARE", 0, 12, false);
					drawText("TRAPPED INSIDE!", 0, 13, false);	
					playAudio("audio/kitmeow1.wav");					
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
				switchGameTo(game);
			}
			
			break;
		
	}

	if (button(A_but)) {
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


void nextFloor() {

	if (++currentFloor == 7) {
		//goto boss
	}
	
	for (int x = 0 ; x < 6 ; x++) {		//Clear next set of condos
		apartmentState[x] = 0;
	}
	
	currentCondo = 0;			//Bud hasn't entered a condo

	hallwayBudSpawn = 10;		//Bud from elevator

	stopWatchClear();
	switchGameTo(goHallway);
	
}

void drawBudStats() {

	drawSprite(4, 4, 0x01F8, 4, false, false);	//face
	drawSprite(12, 4, 0x01F9, 0, false, false);	//X
	drawSpriteDecimal(budLives, 20, 4, 0);		//lives

	for (int x = 0 ; x < 3 ; x++) {
		if (budPower > x) {
			drawSprite((x << 3) + 4, 14, 0x01FB, 4, false, false);
		}
		else {
			drawSprite((x << 3) + 4, 14, 0x01FA, 4, false, false);
		}			
	}		
	
}	

void budDamage() {

	budPower--;			//Dec power
	
	if (budPower == 0) {	
		budStunned = 10;				//Prevent object re-triggers during death
		menuTimer = 30;					//One second
		playAudio("audio/buddead.wav");
		budState = dead;
		movingObjects(false);			//Freeze objects
		return;		
	}
	else {
		budStunned = 10;				//15 frame stun knockback then 1 sec invinc
		budStunnedDir = !budDir;
		playAudio("audio/budhit.wav");
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
		
			if (object[g].animate == 8) {			//Object must be falling max speed to kill a robot
			
				bool hit = false;
				
				if (currentMap == condo) {				
					if (object[index].hitBox(object[g].xPos, object[g].yPos, object[g].xPos + (object[g].width << 3), object[g].yPos + (object[g].height << 3)) == true) {
						hit = true;
					}
				}
				else {
					if (object[index].hitBoxSmall(object[g].xPos, object[g].yPos, object[g].xPos + (object[g].width << 3), object[g].yPos + (object[g].height << 3)) == true) {
						hit = true;
					}
	
				}
							
				if (hit == true) {
					playAudio("audio/glass_1.wav");	//Just the start of this (probably mix with object smash)
					fallingObjectState[x] = false;		//Remove it from fall list
					object[g].active = false;			//Destory falling object				
					object[index].state = 200;			//Short circuit then blow up robot
					object[index].animate = 0;
				}			
			}
	
		}
	}	
	
}

void movingObjects(bool state) {
	
	for (int x = 0 ; x < maxThings ; x++) {			//Find first open slot
		if (object[x].active == true) {		//If object exists and is a robot, turn movement on/off
			object[x].moving = state;
		}
	}	
	
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

		// bool centered = true;
		
		// if (budX < 52) {
			// budX++;
			// centered = false;
		// }
		// if (budX > 52) {
			// budX--;
			// centered = false;
		// }
		// if (budY < 55) {
			// budY++;
			// centered = false;
		// }
		// if (budY > 55) {
			// budY--;
			// centered = false;
		// }
		
		// if (centered == true) {
			// switchGameTo(goJail);
		// }		
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
				setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 16, budWorldY + 8);			//2x2 tile hit box (his ears don't count)
	
				if (budDir == false) {			//Right
					drawSprite(budX, budY, 0 + tail, 31, budPalette, budDir, false);		//Draw animated tail on fours		
					drawSprite(budX + 8, budY, 7, 18, budPalette, budDir, false);		//Front feet
					drawSprite(budX, budY - 8, 6, 17, budPalette, budDir, false);		//Back
					
					if (blink < 35) {
						drawSprite(budX + 8, budY - 8, 7, 17, budPalette, budDir, false);
					}
					else {
						drawSprite(budX + 8, budY - 8, 6, 16, budPalette, budDir, false);	//Blinking Bud!
					}

					drawSprite(budX + 8, budY - 16, 7, 16, budPalette, budDir, false);	//Ear tips					
					
				}
				else {							//Left
					drawSprite(budX + 8, budY, 0 + tail, 31, budPalette, budDir, false);		//Draw animated tail on fours		
					drawSprite(budX, budY, 7, 18, budPalette, budDir, false);		//Front feet
					drawSprite(budX + 8, budY - 8, 6, 17, budPalette, budDir, false);		//Back
					
					if (blink < 35) {
						drawSprite(budX, budY - 8, 7, 17, budPalette, budDir, false);
					}
					else {
						drawSprite(budX, budY - 8, 6, 16, budPalette, budDir, false);	//Blinking Bud!
					}

					drawSprite(budX, budY - 16, 7, 16, budPalette, budDir, false);	//Ear tips					
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
					setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 24, budWorldY + 8);
					budMoveRightC(3);
				}
				else {				
					drawSprite(budX - 8, budY - 8, 0, 16 + offset, 3, 2, budPalette, budDir, false);	//Falling facing left
					setBudHitBox(budWorldX - 8, budWorldY - 8, budWorldX + 16, budWorldY + 8);
					budMoveLeftC(3);
				}		
			}
			else {										//Running on ground animation
	
				if (budDir == false) {		
					drawSprite(budX, budY - 8, 0, 16 + (budFrame << 1), 3, 2, budPalette, budDir, false);	//Running
					setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 24, budWorldY + 8);
					budMoveRightC(3);		
				}
				else {				
					drawSprite(budX - 8, budY - 8, 0, 16 + (budFrame << 1), 3, 2, budPalette, budDir, false);	//Running
					setBudHitBox(budWorldX - 8, budWorldY - 8, budWorldX + 16, budWorldY + 8);
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
				setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 16, budWorldY + 8);
				setBudAttackBox(budWorldX - 16, budWorldY - 12, budWorldX, budWorldY);
			}
			else {							//Right
				drawSprite(budX, budY - 16, 8, 16 + (budSwipe * 3), 4, 3, budPalette, budDir, false);
				setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 16, budWorldY + 8);
				setBudAttackBox(budWorldX + 16, budWorldY - 12, budWorldX + 32, budWorldY);
			}	
			break;

		case damaged:
			if (budStunnedDir == true) {	//Stunned rolling left
				drawSprite(budX, budY - 8, 14, 16 + (budFrame << 1), 2, 2, budPalette, false, false);	//Rolls left, but is facing right, rolling backwards
				setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 16, budWorldY + 8);
				budMoveLeftC(4);
			}
			else {							//Stunned rolling right
				drawSprite(budX, budY - 8, 14, 16 + (budFrame << 1), 2, 2, budPalette, true, false);
				setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 16, budWorldY + 8);
				budMoveRightC(4);				
			}
			if (animateBud) {
				if (++budFrame > 3) {
					budFrame = 0;
				}					
			}
			if (--budStunned == 0) {
				budState = rest;
				budBlink = 30;			//After the stun 1 sec invincible
			}			
			break;

		case exiting:		
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
						playAudio("audio/doorOpen.wav");
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
						playAudio("audio/elevDing.wav");
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
	
		case entering:		
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

		if (button(A_but)) {
			//playAudio("audio/hitModem.wav");
			
			// levelComplete = true;
			// elevatorOpen = true;
			// drawHallwayElevator(64);
			// redrawMapTiles();
			// kittenMessageTimer = 60;		//In hallway logic, use this var to print GOTO THE ELEVATOR
		
			kittenCount = kittenTotal;
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
					playAudio("audio/doorOpen.wav");
					drawHallwayDoorOpen(doorTilePos[temp], 0);
					//populateDoor(doorTilePos[temp], temp + 1, true);		//Stuffs the high byte with BCD floor/door number
					redrawMapTiles();
					//redrawCurrentHallway();
					budState = entering;
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

		if (checkElevator() == 1 && budState != entering) {								//Bud standing in front of elevator?
			
			if (elevatorOpen == true) {							//Ready for the next level?

				if (button(up_but)) {
					playAudio("audio/elevDing.wav");
					budState = entering;
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
		
		budX -= speed;
		budWorldX -= speed;
		xWindowBudFine -= speed;
		
		if (xWindowBudFine < 0) {				//Passed tile boundary?
			xWindowBudFine += 8;			//Reset fine bud screen scroll
			
			xWindowBudToTile = windowCheckLeft(xWindowBudToTile);
		}	

		if (budX < 8 && currentMap == condo) {
			if (kittenCount == kittenTotal) {
				exitCondo();
			}
			else {
				if (kittenMessageTimer == 0) {
					kittenMessageTimer = 60;	
					playAudio("audio/taunt.wav");
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
		budX += speed;
		budWorldX += speed;
		xWindowBudFine += speed;
		
		if (xWindowBudFine > 7) {				//Passed tile boundary?
			xWindowBudFine -= 8;			//Reset fine bud screen scroll
			
			xWindowBudToTile = windowCheckRight(xWindowBudToTile);
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
			worldX += theSpeed;
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

void setBudHitBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
	
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
			++minutes;
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
	
	setCoarseYRollover(0, 14);   //Sets the vertical range of the tilemap, rolls back to 0 after 29

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
	
	editDisableCursor = false;
	displayPause = false;   		//Allow core 2 to draw
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

	fillTiles(0, 0, 14, 8, ' ', 3);			//Clear area

	if (cursorBlink & 0x01) {
		drawText(">", 0, editMenuSelectionY, false);		
	}
	
	drawText("COPY TO SLOT", 1, 0, false);
	drawDecimal(currentCopyBuffer, 14, 0);	
	drawText("PASTE + SLOT", 1, 1, false);	
	drawDecimal(currentCopyBuffer, 14, 1);	
	drawText("UNDO LAST", 1, 2, false);
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
	
	Serial.print("Game level filename:");
	Serial.println(mapFileName);

}

void updateMapFileNameEdit() {				//Call thise in edit/game mode before calling save/load
	
	mapFileName[12] = fileNameFloor;
	mapFileName[14] = fileNameCondo;

	if (editType == true) {				//Condo? Change the numbers on the open door on the left
		Serial.print("CONDO MODE: ");
		condoMap[8][2] = 0x80 + (fileNameFloor - 48);		//Update the number on the open door
		condoMap[8][3] = 0x80 + (fileNameCondo - 48);		
	}
	else {
		Serial.print("HALLWAY MODE: ");
		currentFloor = fileNameFloor - 48;				//Convert ASCII to a number and change the Floor # on top of the elevator
		
		if (cutscene == false) {
			populateEditDoors();
			drawHallwayElevator(64);					
		}
	}

	Serial.print("Edit filename:");
	Serial.println(mapFileName);

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
	
	for (int x = 0 ; x < maxThings ; x++) {		//Robot scan (next priority)
		if (object[x].active == true) {
		
			if (object[x].category == 0) {		//Gameplay object?		
				if (object[x].type < 4) {		//Evil robot? Save
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
		Serial.println("FILE NOT FOUND");
		messageTimer = 30;
		redrawEditWindow();	
		return;		
	}
	
	Serial.println("LOADING FILE");
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

	Serial.print(mapWidth * 15);
	Serial.println(" TILES LOADED");

	if (readByte() == 128) {				//128 = end of tile data. Old files don't have this so skip objects if not found

		uint8_t objectCount = readByte();			//Get # of objects saved in this file

		highestObjectIndex = objectCount;			//Set this as our scan limit

		Serial.print(objectCount, DEC);
		Serial.println(" OBJECTS LOADED");
	
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
		Serial.println("LOAD COMPLETE");
		messageTimer = 30;
		redrawEditWindow();					
	}
	else {
		closeFile();	
		redrawEditWindow();		
		makeMessage("LOAD ERROR");
		Serial.println("LOAD ERROR");
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
			setButtonDebounce(A_but, false, 0);
			bButtonFlag = false;
			clearMessage();
			
			if (cursorDrops == 2) {						//Dropping objects, enable debounce so they don't pile up
				editDisableCursor = false;
				editAaction = 10;
				setButtonDebounce(A_but, true, 0);
				makeMessage("PLACE OBJECT");
				messageTimer = 20;						
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
				break;
				
				case 1:
					makeMessage("TILE TYPE");
					messageTimer = 20;
					redrawEditWindow();					
				break;
				
				case 2:
					makeMessage("OBJECT PLACE");
					messageTimer = 20;
					redrawEditWindow();					
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
				sentryLeft = (editWindowX << 3) + (drawX << 3);
				makeMessage("SENTRY RIGHT?");
				messageTimer = 20;	
				redrawEditWindow();		
				editAaction = 12;
			break;
			
			case 12:					//Set sentry right edge
				sentryRight = (editWindowX << 3) + (drawX << 3) + 8;	//The right edge of the recticle is the edge (+8)
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

void placeObjectInMap() {

	for (int x = 0 ; x < maxThings ; x++) {			//Find first open slot
		if (object[x].active == false) {
			objectIndexNext = x;
			break;
		}				
	}
	
	Serial.print("Placing object #");
	Serial.println(objectIndexNext);
	
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

int placeObjectInMap(int xPos, int yPos, int sheetX, int sheetY, int width, int height, int palette, int dir, int category, int type) {

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


void testAnimateObjects(bool onOff) {

	fallListClear();

	for (int x = 0 ; x < maxThings ; x++) {			//Find first open slot
		if (object[x].active == true && object[x].category == 0) {		//If object exists and is a robot, turn movement on/off
			object[x].moving = onOff;
		}
	}	
	
}

void clearObjects() {

	for (int x = 0 ; x < maxThings ; x++) {			//Find first open slot
		object[x].active = false;
		object[x].state = 0;	
		object[x].subAnimate = 0;
		object[x].animate = 0;
	}	
	
	highestObjectIndex = 0;
	
}

void editorDrawObjects() {
	
	for (int g = 0 ; g < maxThings ; g++) {
		
		if (object[g].active) {
			object[g].scan(editWindowX << 3, 0);			//See if object visible
			
			if (objectDropCategory == 5 && eraseFlag == true) {
				
				if (object[g].hitBox((editWindowX << 3) + (drawX << 3), drawY << 3, (editWindowX << 3) + (drawX << 3) + 8, (drawY << 3) + 8) == true) {	//Does the 8x8 reticle fall within the object bounds?
					object[g].active = 0;	
					eraseFlag = false;
					Serial.print("Erasing object #");
					Serial.println(g);
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
				testAnimateState = !testAnimateState;
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
	drawText(")  TYPE  *", 5, 1, false);
	
// int objectDropCategory = 1;				//0= gameplay (enemies and greenies), 1 = bad art 4x2, 2 = small 2x2, 3= tall 2x3, 4= appliance 3x2, 5= eraser
// int objectDropType= 0;					//index of object within its category

//0= tall robot 1=can robot, 2 = flat robot, 3= dome robot, 4= kitten, 5= greenie (greenies on bud sheet 1)
//int robotSheetIndex[5][2] = { {0, 0}, {0, 8}, {0, 12}, {8, 0}, {6, 3} };
//int robotTypeSize[5][2] = { {2, 6}, {3, 3}, { 4, 2 }, { 4, 3 }, { 2, 2 } };		//Size index


	switch(objectDropCategory) {
		       //0123456789ABCDE
		case 0:
			drawText("GAME PLAY", 5, 2, false);
			objectSheetIndex[objectDropCategory][0] = robotSheetIndex[selectRobot][0];			//Get sheet XY and offset by page 3
			
			if (selectRobot == 5) {
				objectSheetIndex[objectDropCategory][1] = robotSheetIndex[selectRobot][1] + 16;	
				objectPlacePalette = 5;		//Always green
			}
			else {
				objectSheetIndex[objectDropCategory][1] = robotSheetIndex[selectRobot][1] + 48;	
			}
						
			objectTypeSize[objectDropCategory][0] = robotTypeSize[selectRobot][0];
			objectTypeSize[objectDropCategory][1] = robotTypeSize[selectRobot][1];
			
			drawSprite(0, 0, objectSheetIndex[objectDropCategory][0], objectSheetIndex[objectDropCategory][1], objectTypeSize[objectDropCategory][0], objectTypeSize[objectDropCategory][1], objectPlacePalette, objectPlaceDir, false);	
		break;			   
		case 1:
			drawText("BAD ART", 5, 2, false);		//type 0-7			
			objectSheetIndex[objectDropCategory][0] = ((objectDropType & 0x01) << 2);
			objectSheetIndex[objectDropCategory][1] = ((objectDropType >> 1) << 1) + 32;	
			drawSprite(0, 0, objectSheetIndex[objectDropCategory][0], objectSheetIndex[objectDropCategory][1], objectTypeSize[objectDropCategory][0], objectTypeSize[objectDropCategory][1], objectPlacePalette, objectPlaceDir, false);	
		break;
		case 2:
			drawText("KITCHEN", 5, 2, false);		//type 0-15
			objectSheetIndex[objectDropCategory][0] = ((objectDropType & 0x03) << 1) + 8;
			objectSheetIndex[objectDropCategory][1] = ((objectDropType >> 2) << 1) + 32;	
			drawSprite(0, 0, objectSheetIndex[objectDropCategory][0], objectSheetIndex[objectDropCategory][1], objectTypeSize[objectDropCategory][0], objectTypeSize[objectDropCategory][1], objectPlacePalette, objectPlaceDir, false);	
			
		break;		
		case 3:
			drawText("TALL STUFF", 5, 2, false);	//type 0-3
			objectSheetIndex[objectDropCategory][0] = ((objectDropType & 0x03) << 1) + 8;
			objectSheetIndex[objectDropCategory][1] = 32 + 8;
			drawSprite(0, 0, objectSheetIndex[objectDropCategory][0], objectSheetIndex[objectDropCategory][1], objectTypeSize[objectDropCategory][0], objectTypeSize[objectDropCategory][1], objectPlacePalette, objectPlaceDir, false);				
		break;		
		case 4:
			drawText("APPLIANCE", 5, 2, false);		//type 0-3
			objectSheetIndex[objectDropCategory][0] = ((objectDropType & 0x01) << 2) + 8;
			objectSheetIndex[objectDropCategory][1] = 32 + 11 + ((objectDropType >> 1) << 1);
			drawSprite(0, 0, objectSheetIndex[objectDropCategory][0], objectSheetIndex[objectDropCategory][1], objectTypeSize[objectDropCategory][0], objectTypeSize[objectDropCategory][1], objectPlacePalette, objectPlaceDir, false);				
		break;	
		case 5:
			drawText("ERASE", 5, 2, false);		//type 0-3
			drawSprite(16, 16, 0x2C, 3, false, false);
		break;	

		case 6:
			drawText("TEST ANI", 5, 2, false);		//type 0-3			
		break;
		
	}
	
	drawText("<  ITEM  >", 5, 4, false);
	
	drawText("A=FLIP H", 5, 6, false);		//Starting dir of robots, no effect on greenes
	drawText("C=COLOR", 5, 7, false);		//No effect on greenies because, duh, they're GREEN ONLY
		    //0123456789ABCDE	
	drawText("SEL=FINE/COARSE", 0, 8, false);		//No effect on greenies because, duh, they're GREEN ONLY
	
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
			
	writeByte(object[which].extraA);	
	writeByte(object[which].extraB);	
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
			
	object[which].extraA = readByte();		
	object[which].extraB = readByte();		
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

