#include <gameBadgePico.h>
#include "thingObject.h"
//Your defines here------------------------------------
#include "hardware/pwm.h"
#include <WiFi.h>

struct repeating_timer timer30Hz;			//This runs the game clock at 30Hz

bool wifiConnected = false;
int wifiPower = 0;

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

enum movement { rest, starting, walking, running, jumping, moving, turning, stopping, entering, exiting };

movement budState = rest;

int budFrame = 0;
int budSubFrame = 0;
bool budDir = false;	//True = going left
int budX = 56;
int budY = 8;

uint16_t budWorldX = 8 * (6 + 7);			//Bud's position in the world (not the same as screen or tilemap)
uint16_t budWorldY = 8;

uint16_t budWx1, budWx2, budWy1, budWy2;	//Upper left and lower right edge of Bud's hit box. It changes depending on what he's doing

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
int8_t xWindowCoarseTile = 6 + 7;		//Where Bud is (on center of screen, needs to be dynamic)
int8_t xStripLeft = 0;				    //Strip redraw position on left (when moving left)
int8_t xStripRight = 26;					//Strip redraw position on right (when moving right)

uint16_t xLevelCoarse = 6;				//In the tilemap, the left edge of the current positiom (coarse)
uint16_t xLevelStripLeft = 0;				//Strip redraw position on left (when moving left)
uint16_t xLevelStripRight = 26;			//Strip redraw position on right (when moving right)

//Define flags for tiles. Use setTileType() to add these to tiles after you've drawn them
#define tilePlatform		0x80
#define tileBlocked			0x40

#define hallBlank		0
#define hallCounter		2
#define hallLeftWall	3
#define hallRightWall	4
#define hallElevator	5
#define hallCallButton	6
#define hallDoorClosed	10
#define hallDoorOpen	11

#define bigRobot			1
#define chandelier			2
#define chandelierSmashed 	3
#define roomba				10
#define greenie				255

bool chandelierFalling = false;
int cWF = 0xFF;						//Stands for chandelier which falling it's short so functions don't get insane

int currentFloor = 1;

int doorTilePos[6] = { 16, 34, 52, 92, 110, 128 };

char apartmentState[7] = { 0, 0, 0, 0, 0, 0, 0 };	//Zero index not used, starts at 1 goes to 6

bool elevatorOpen = true;

int inArrowFrame = 0;

#define maxThings		64
thingObject thing[maxThings];

int totalThings = 0;
int lastRemoved = 0xFF;
int lastAdded = 0xFF;

bool gameActive = false;
bool isDrawn = false;													//Flag that tells new state it needs to draw itself before running logic

enum stateMachine { bootingMenu, splashScreen, mainMenu, wifiMode, passwordEntry, login, gameRunning };		//State machine of the badge (boot, wifi etc)
stateMachine badgeState = bootingMenu;

enum stateMachineGame { bootingGame, titleScreen, game };					//State machine of the game (under stateMachine = game)
stateMachineGame gameState = bootingGame;

uint16_t menuTimer;

uint8_t cursorX, cursorY, cursorAnimate;

uint8_t soundTimer = 0;

int networksFound = 0;

uint8_t passChar[3];

uint8_t passCharStart[3] = { 32, 64, 96 };
uint8_t passCharEnd[3] = { 63, 95, 127 };

char ssidString[32];
char passwordString[32];
int passwordCharEdit = 0;

WiFiMulti multi;

const char* ssid = "Wireless Hub #42";
const char* password = "BAD42FAB42";

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

	if (displayPause == false) {   
		if (button(start_but)) {      //Button pressed?
		  paused = true;
		  Serial.println("PAUSED");
		}      
	}
	else {
		if (button(start_but)) {      //Button pressed?
		  displayPause = false;
		  Serial.println("UNPAUSED");
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
	
		case bootingMenu:
			if (menuTimer > 0) {					//If timer active, decrement
				--menuTimer;
			}
			
			if (menuTimer == 0) {					//First boot, or did timer reach 0?
				if (loadRGB("NEStari.pal")) {					//Try and load master pallete from root dir
					//Load success? Load the rest
					loadPalette("UI/basePalette.dat");          //Load palette colors from a YY-CHR file. Can be individually changed later on
					loadPattern("UI/logofont.nes", 0, 256);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)					
					switchMenuTo(splashScreen);					//Progress to splashscreen					
				}
				else {
					menuTimer = 30;					//Try again in 1 second
				}							
			}

			break;
			
		case splashScreen:
			if (isDrawn == false) {
				drawSplashScreen();
			}	
			if (--menuTimer == 0) {					//Dec timer and goto main menu after timeout
				fillTiles(0, 0, 14, 14, 0, 3);
				loadPattern("UI/baseFont.nes", 0, 512);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)
				switchMenuTo(mainMenu);
			}
			break;
			
			
		case mainMenu:
			if (isDrawn == false) {
				drawMainMenu();
			}	
			if (wifiConnected == true) {						//Update Wifi status
				drawTile(13, 0, 0x09, 3, 0);					//On/off
			}
			else {
				drawTile(13, 0, 0x20, 3, 0);
			}
			drawTile(14, 0, 0x0A + wifiPower, 3, 0);			//Power

			if (++menuTimer > 3) {
				menuTimer = 0;
				cursorAnimate += 0x10;
				if (cursorAnimate > 0x20) {
					cursorAnimate = 0x02;
				}
			}
			
			fillTiles(cursorX, 4, cursorX, 8, ' ', 3);
			
			drawTile(cursorX, cursorY, cursorAnimate, 3, 0);			//Animated arrow

			if (button(up_but)) {
				if (cursorY > 4) {
					cursorY--;
					pwm_set_freq_duty(6, 493, 25);
					soundTimer = 10;
				}				
			}
			if (button(down_but)) {
				if (cursorY < 8) {
					cursorY++;
					pwm_set_freq_duty(6, 520, 25);
					soundTimer = 10;
				}				
			}
			
			if (button(A_but)) {
			
				switch(cursorY) {
						
					case 4:
						switchMenuTo(wifiMode);
					break;
					
					case 5:
						paused = true;
						setupHallway();
						gameActive = true;
					break;
					
				}
			
				
			}
			

			break;
			
		case wifiMode:
			if (isDrawn == false) {
				drawWifiScreen();
			}		

			if (++menuTimer > 3) {
				menuTimer = 0;
				cursorAnimate += 0x10;
				if (cursorAnimate > 0x20) {
					cursorAnimate = 0x02;
				}
			}
			
			fillTiles(0, 1, 0, 13, ' ', 3);						//Erase arrows
			
			drawTile(0, cursorY, cursorAnimate, 0, 0);			//Animated arrow

			if (button(up_but)) {
				if (cursorY > 1) {
					cursorY--;
					pwm_set_freq_duty(6, 493, 25);
					soundTimer = 10;
				}				
			}
			if (button(down_but)) {
				if (cursorY < (networksFound)) {
					cursorY++;
					pwm_set_freq_duty(6, 520, 25);
					soundTimer = 10;
				}				
			}


			if (button(A_but)) {	
				for (int x = 2 ;  x < 31 ; x++) {
					
					char theChar = getTileValue(x, cursorY);
					
					if (theChar == 0x04) {
						break;
					}
					ssidString[x - 2] = theChar;							//Scan the chars into the SSID string
					ssidString[x - 1] = 0;									//Add zero terminator
					
				}

			
				switchMenuTo(passwordEntry);					
			}
	
			if (button(B_but)) {	
				switchMenuTo(mainMenu);					
			}
	
			break;
			
		case passwordEntry:
			if (isDrawn == false) {
				drawPasswordEntry();
			}		

			for (int sel = 0 ; sel < 3 ; sel++) {
			
				uint8_t which = passChar[sel];
			
				for (int x = 0 ; x < 15 ; x++) {
				
					drawTile(x, 7 + (sel * 2), which, 2, 0);
					
					if (++which > passCharEnd[sel]) {
						which = passCharStart[sel];
					}				
					
				}

			
			}
			setTilePalette(6, 7, 3);
			setTilePalette(6, 9, 3);
			setTilePalette(6, 11, 3);

			for (int x = 0 ; x < passwordCharEdit ; x++) {			
				drawTile(x, 3, passwordString[x], 3, 0);			
			}
			
			menuTimer++;
			
			if (menuTimer & 0x08) {
				drawTile(passwordCharEdit, 3, '_', 3, 0);
			}
			else {
				drawTile(passwordCharEdit, 3, ' ', 3, 0);
			}

			if (button(A_but)) {
				passwordString[passwordCharEdit++] = getTileValue(6, 7);
				passwordString[passwordCharEdit] = 0;	
			}
			if (button(B_but)) {
				passwordString[passwordCharEdit++] = getTileValue(6, 9);	
				passwordString[passwordCharEdit] = 0;		
			}			
			if (button(C_but)) {
				passwordString[passwordCharEdit++] = getTileValue(6, 11);
				passwordString[passwordCharEdit] = 0;	
			}		

			if (button(select_but)) {	
				switchMenuTo(login);
			}	
			
			if (button(up_but)) {	
				if (passwordCharEdit > 0) {
					passCharStart[passwordCharEdit] = 0;				//Erase zero terminate
					drawTile(passwordCharEdit, 3, ' ', 3, 0);	
					passwordCharEdit--;					
				}
			}	
			
			if (button(left_but)) {					
				for (int sel = 0 ; sel < 3 ; sel++) {
					if (--passChar[sel] < passCharStart[sel]) {
						passChar[sel] = passCharEnd[sel];
					}
				}		
			}	
			
			if (button(right_but)) {		
				for (int sel = 0 ; sel < 3 ; sel++) {
					if (++passChar[sel] > passCharEnd[sel]) {
						passChar[sel] = passCharStart[sel];
					}
				}				
			}
		
			break;			
			
			
		case login:
			if (isDrawn == false) {
				drawLoginScreen();
			}
			
			if (wifiConnected == false) {
				
				if (++menuTimer == 30) {
					menuTimer = 0;
					if (multi.run() == WL_CONNECTED) {
						drawText("SUCCESS!", 0, 5, false);	
						wifiConnected = true;
					}
					else {
						drawText("FAIL!", 0, 5, false);
					}								
				}			
				
			}
			
			if (button(B_but)) {
				switchMenuTo(mainMenu);		
			}	
			
			break;
			
		case gameRunning:
			//Transition to game
			break;
		
		
	}
	
	if (soundTimer > 0) {
		if (--soundTimer == 0) {
			pwm_set_freq_duty(6, 0, 0);
		}
	}

}

void switchMenuTo(stateMachine x) {		//Switches state of menu

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
	
	playAudio("gameBadge.wav");		//Splash audio
	
}

void drawMainMenu() {

	fillTiles(0, 0, 14, 14, ' ', 3);			//CLS
	
	drawText("MAIN MENU", 0, 0, false);

	drawText("Connect WIFI", 2, 4, false);
	drawText("Run GAME", 2, 5, false);
	
	drawText("A=SELECT", 0, 14, false);

	cursorX = 1;
	cursorY = 4;
	cursorAnimate = 0x02;
	menuTimer = 0;
	
	setWindow(0, 0);

	setButtonDebounce(up_but, true, 1);			//Set debounce on d-pad for menu select
	setButtonDebounce(down_but, true, 1);
	setButtonDebounce(left_but, true, 1);
	setButtonDebounce(right_but, true, 1);
	
	displayPause = false;   		//Allow core 2 to draw
	isDrawn = true;
	
}

void drawWifiScreen() {
	
	fillTiles(0, 0, 14, 14, ' ', 3);			//CLS
	
	cursorX = 1;
	cursorY = 1;
	cursorAnimate = 0x02;
	menuTimer = 0;	

	int y = 1;

	auto cnt = WiFi.scanNetworks();
	
	networksFound = 0;
	
	if (!cnt) {
		drawText("NO NETWORKS", 0, 0, false);
	}
	else {
		drawText("NETWORKS:", 0, 0, false);
		drawDecimal(cnt, 10, 0);

		for (auto i = 0; i < cnt; i++) {
			uint8_t bssid[6];
			WiFi.BSSID(i, bssid);
			//Serial.printf("%32s %5s %17s %2d %4d\n", WiFi.SSID(i), encToString(WiFi.encryptionType(i)), macToString(bssid), WiFi.channel(i), WiFi.RSSI(i));
			fillTiles(1, y, 31, y, 0x04, 3);
			drawText(WiFi.SSID(i), 2, y, false);
			drawTile(1, y, rssiToIcon(WiFi.RSSI(i)), 3, 0);			//Draw strength
			networksFound++;
			y++;		  
		}
	}
	
	displayPause = false;   		//Allow core 2 to draw
	isDrawn = true;
	
}

void drawPasswordEntry() {
	
	fillTiles(0, 0, 14, 14, ' ', 2);			//CLS
	
	drawText(ssidString, 0, 0, false);
	drawText("Enter password:", 0, 1, false);
	
	drawText("UP=backspace", 0, 5, false);
		
	drawTile(6, 8, 0, 0, 0);
	drawText("-A press", 7, 8, false);	

	drawTile(6, 10, 0, 0, 0);
	drawText("-B press", 7, 10, false);

	drawTile(6, 12, 0, 0, 0);
	drawText("-C press", 7, 12, false);
	
	drawText("SELECT=login", 2, 14, false);

	
	passChar[0] = 32;
	passChar[1] = 64;
	passChar[2] = 96;
	
	displayPause = false;   		//Allow core 2 to draw
	isDrawn = true;
	
}

void drawLoginScreen() {
	
	fillTiles(0, 0, 14, 14, ' ', 2);			//CLS
	
	drawText("Connecting to:", 0, 0, false);	
	drawText(ssidString, 0, 1, false);
	drawText("Password:", 0, 2, false);	
	drawText(passwordString, 0, 3, false);

	multi.addAP(ssid, password);
	
	menuTimer = 0;
	
	displayPause = false;   		//Allow core 2 to draw
	isDrawn = true;		

}

int rssiToIcon(int level) {
	
	if (level < -100) {
		return 0x0A;				//Connected but BAD
	}

	int value = (level - (-100));		//Invert and get delta
	
	value /= 10;						//Divide by 10

	value += 0x0B;						//Add to char map
	
	if (value > 0x0F) {					//Max bars
		value = 0x0F;
	}
	
	return value;
	
}


void gameFrame() { //--------------------This is called at 30Hz. Your main game state machine should reside here

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

	bool animateBud = false;			//Bud is animated "on twos" (every other frame at 30HZ, thus Bud animates at 15Hz)
	
	if (++budSubFrame > 1) {
		budSubFrame = 0;
		animateBud = true;				//Set animate flag. Bud still moves/does logic on ones
	}		

	int jumpGFXoffset = 6;						//Jumping up	
	
	bool budNoMove = true;
	
	switch(budState) {
	
		case rest:								//Rest = not moving left or right. Can still be jumping or falling
			if (jump > 0) {
				int offset = 6;					//Jumping up gfx (default)
				
				if (jump == 128) {
					offset = 8;					//Falling down gfx
				}			
				if (budDir == false) {														//Falling facing right	
					drawSprite(budX, budY - 8, 0, 16 + offset, 3, 2, 4, budDir, false);
					setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 24, budWorldY + 8);
				}
				else {				
					drawSprite(budX - 8, budY - 8, 0, 16 + offset, 3, 2, 4, budDir, false);	//Falling facing left
					setBudHitBox(budWorldX - 8, budWorldY - 8, budWorldX + 16, budWorldY + 8);
				}				
			}
			else {
				setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 16, budWorldY + 8);			//2x2 tile hit box (his ears don't count)
	
				if (budDir == false) {			//Right
					drawSprite(budX, budY, 0 + tail, 31, 4, budDir, false);		//Draw animated tail on fours		
					drawSprite(budX + 8, budY, 7, 18, 4, budDir, false);		//Front feet
					drawSprite(budX, budY - 8, 6, 17, 4, budDir, false);		//Back
					
					if (blink < 35) {
						drawSprite(budX + 8, budY - 8, 7, 17, 4, budDir, false);
					}
					else {
						drawSprite(budX + 8, budY - 8, 6, 16, 4, budDir, false);	//Blinking Bud!
					}

					drawSprite(budX + 8, budY - 16, 7, 16, 4, budDir, false);	//Ear tips					
					
				}
				else {							//Left
					drawSprite(budX + 8, budY, 0 + tail, 31, 4, budDir, false);		//Draw animated tail on fours		
					drawSprite(budX, budY, 7, 18, 4, budDir, false);		//Front feet
					drawSprite(budX + 8, budY - 8, 6, 17, 4, budDir, false);		//Back
					
					if (blink < 35) {
						drawSprite(budX, budY - 8, 7, 17, 4, budDir, false);
					}
					else {
						drawSprite(budX, budY - 8, 6, 16, 4, budDir, false);	//Blinking Bud!
					}

					drawSprite(budX, budY - 16, 7, 16, 4, budDir, false);	//Ear tips					
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
			if (budDir == false) {		
				drawSprite(budX, budY - 8, 0, 16 + (budFrame << 1), 3, 2, 4, budDir, false);	//Running
				setBudHitBox(budWorldX, budWorldY - 8, budWorldX + 24, budWorldY + 8);
				budMoveRight(3);		
			}
			else {				
				drawSprite(budX - 8, budY - 8, 0, 16 + (budFrame << 1), 3, 2, 4, budDir, false);	//Running
				setBudHitBox(budWorldX - 8, budWorldY - 8, budWorldX + 16, budWorldY + 8);
				budMoveLeft(3);		
			}
			if (animateBud) {
				if (++budFrame > 6) {
					budFrame = 0;
				}					
			}			
			break;	
	
		case entering:
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
				}
			}		
		
			break;
	
	
	
	}

	if (jump & 0x7F) {					//Jump is set, but the MSB (falling) isn't?
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
	
		if (getTileType(xWindowCoarseTile, (budY >> 3)) == 0 && getTileType(budTileCheck(1), (budY >> 3)) == 0) {	//Nothing below?
		
			if (jump == 0) {			//Weren't already falling?
				velocikitten = 2;		//Means we walked off a ledge, reset velocity
				jump = 128;				//Set falling state
				//budState = jumping;
			}

			budY += velocikitten;					//Fall
			budWorldY += velocikitten;
			if (velocikitten < 8) {
				velocikitten++;
			}
		}
		else {
			if (jump == 128) {				//Were we falling? Clear flag
				jump = 0;					//Something below? Clear jump flag
				budY &= 0xF8;
				budWorldY &= 0xF8;
				budState = rest;
			}		
		}
	
	}

	if (button(C_but)) {
		//playAudio("audio/back.wav");
		//drawText("and it's been the ruin of many of a young boy. And god, I know, I'm one...", 0, 12, true);
		//dutyOut = 50;
		//pwm_set_freq_duty(12, 493, 25);
		
		if (jump == 0) {
			jump = 1;
			velocikitten = 9;
			budNoMove = false;
			//budState = jumping;	
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
	

	if (budNoMove == true && budState == moving) {		//Was Bud moving, but then d-pad released? Bud comes to a stop
		budState = rest;
		budFrame = 0;
	}
		
	
	
	
	thingLogic();
	

	
	setWindow((xWindowCoarse << 3) | xWindowFine, yPos);			//Set scroll window

	// setWindowSlice(0, xPos << 1); 			//Last 2 rows set no scroll
	// setWindowSlice(1, xPos << 1); 			//Last 2 rows set no scroll	
	// setWindowSlice(2, xPos << 1); 			//Last 2 rows set no scroll
	// setWindowSlice(3, xPos << 1); 			//Last 2 rows set no scroll		
	//setWindowSlice(14, xPos << 1); 


	moveJump = false;

	
    uint slice_num = pwm_gpio_to_slice_num(14);
    uint chan = pwm_gpio_to_channel(14);
	
	if (button(select_but)) {

	  pwm_set_freq_duty(6, 0, 0);	  
	  pwm_set_freq_duty(8, 0, 0);
	  pwm_set_freq_duty(10, 0, 0);
	  pwm_set_freq_duty(12, 0, 0);
	  fillTiles(0, 0, 31, 31, ' ', 0);
	  
	}


	int tempCheck = checkDoors();			//See if Bud is standing in front of a door
	
	if (tempCheck > -1 && budState != entering) {					//He is? tempCheck contains a index # of which one (0-5)
		
		if ((apartmentState[tempCheck] & 0xC0) == 0x00) {	//If apartment cleared or door already open bits set, no action can be taken

			if (button(up_but)) {
				apartmentState[tempCheck] |= 0x40;								//Set the door open bit
				playAudio("audio/doorOpen.wav");
				populateDoor(doorTilePos[tempCheck], tempCheck + 1, true);		//Stuffs the high byte with BCD floor/door number
				redrawCurrentHallway();
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

	// if (button(up_but)) {

		// budState = entering;
		
		// thingRemove(0xFF);
		
		// drawText("0123456789ABC E brute?", 0, 0, true);
		// pwm_set_freq_duty(slice_num, chan, 261, 25);
		
		// Serial.print(" s-");		
		// Serial.print(pwm_gpio_to_slice_num(10), DEC);
		// Serial.print(" c-");
		// Serial.println(pwm_gpio_to_channel(10), DEC);
		// pwm_set_freq_duty(6, 261, 12);
		// pwm_set_freq_duty(8, 277, 50);		
	// }
	
	
	if (button(B_but)) {
		thingAdd(bigRobot, 100, 64);
		//playAudio("audio/getread.wav");
		//playAudio("audio/back.wav");
		//drawText("supercalifraguliosdosis", 0, 14, true);
		//pwm_set_freq_duty(6, 261, 25);
		//pwm_set_freq_duty(8, 277, 50);
	}	
	

	
	
	if (dutyOut > 0) {
		pwm_set_freq_duty(12, 100 - dutyOut, 50);
		dutyOut -= 10;
		if (dutyOut < 1) {
			dutyOut = 0;
			pwm_set_freq_duty(12, 0, 0);
		}
		
	}
	
	//drawSpriteText("GAME OVAH", 16, 16, 3);
	
	drawSpriteDecimal(budWorldX >> 3, 10, 0, 0);		
	drawSpriteDecimal(budWorldX, 10, 8, 0);
	drawSpriteDecimal(budWorldY, 10, 16, 0);	
	drawSpriteDecimal(totalThings, 10, 24, 0);

	
	// drawSpriteDecimal(budX, 8, 0, 0);		
	// drawSpriteDecimal(xWindowBudFine, 8, 8, 0);	
	// drawSpriteDecimal(xWindowCoarseTile, 8, 16, 0);
	// drawSpriteDecimal(xLevelStripRight, 8, 48, 0);		


}

void switchMenuTo(stateMachineGame x) {		//Switches state of menu

	gameState = x;		
	isDrawn = false;
	
}



void setupHallway() {

	displayPause = true;   		//Allow core 2 to draw

	if (loadRGB("NEStari.pal")) {
		//loadRGB("NEStari.pal");                		//Load RGB color selection table. This needs to be done before loading/change palette
		//loadPalette("palette_0.dat");            	//Load palette colors from a YY-CHR file. Can be individually changed later on
		//loadPattern("moon_force.nes", 0, 512);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)

		loadPalette("bud.dat");            	//Load palette colors from a YY-CHR file. Can be individually changed later on
		loadPattern("bud.nes", 0, 1024);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)

		//loadPattern("patterns/basefont.nes", 0, 512);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)

		paused = false;   							//Allow core 2 to draw once loaded
	}

	setButtonDebounce(up_but, true, 1);		//Debounce UP for door entry
	setButtonDebounce(down_but, false, 0);
	setButtonDebounce(left_but, false, 0);
	setButtonDebounce(right_but, false, 0);
	
	setCoarseYRollover(0, 14);   //Sets the vertical range of the tilemap, rolls back to 0 after 29
	
	drawHallway(1);	
	
	displayPause = false;   		//Allow core 2 to draw
	
}

void setBudHitBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
	
	budWx1 = x1;
	budWy1 = y1;	
	budWx2 = x2;
	budWy2 = y2;	
}

bool budMoveLeft(int speed) {

	if ((getTileType(budTileCheck(-1), (budY >> 3)) & tileBlocked) == tileBlocked) {		//Check 1 to the left
		return false;
	}
		
	if (windowMoveLeft(speed) == false) {		//Can window NOT scroll left?
		budX -= speed;
		budWorldX -= speed;
		xWindowBudFine -= speed;
		
		if (xWindowBudFine < 0) {				//Passed tile boundary?
			xWindowBudFine += 8;			//Reset fine bud screen scroll
			
			xWindowCoarseTile = windowCheckLeft(xWindowCoarseTile);
		}			
	}
	
	return true;
}

bool budMoveRight(int speed) {

	if ((getTileType(budTileCheck(2), (budY >> 3)) & tileBlocked) == tileBlocked) {			//Check 2 to the right
		return false;
	}
	
	if (windowMoveRight(speed) == false) {		//Can window NOT scroll left?
		budX += speed;
		budWorldX += speed;
		xWindowBudFine += speed;
		
		if (xWindowBudFine > 7) {				//Passed tile boundary?
			xWindowBudFine -= 8;			//Reset fine bud screen scroll
			
			xWindowCoarseTile = windowCheckRight(xWindowCoarseTile);
		}			
	}
	
	return true;
}

bool windowMoveLeft(int theSpeed) {
	
	if (budX > 48) {
		return false;
	}
	
	xWindowFine -= theSpeed;				//Move fine scroll		
	
	budWorldX -= theSpeed;
	worldX -= theSpeed;
	
	if (xWindowFine < 0) {				//Passed tile boundary?
	
		if (xLevelCoarse == 6) {		//At left edge?
			xWindowFine = 0;			//Clear out fine register, we can't go further left
			worldX += theSpeed;
			budWorldX += theSpeed;
			return false;
		}

		xWindowFine += 8;				//Subtract fine		
		
		xWindowCoarse = windowCheckLeft(xWindowCoarse);
		xWindowCoarseTile = windowCheckLeft(xWindowCoarseTile);
		xStripLeft = windowCheckLeft(xStripLeft);
		xStripRight = windowCheckLeft(xStripRight);

		xLevelStripLeft--;
		xLevelCoarse--;
		xLevelStripRight--;

		xStripDrawing = xStripLeft;			//Where to draw new tiles on tilemap
		
		drawHallwayTiles(xLevelStripLeft);	//Draw the strip
		

	}	
	
	return true;
		
}

bool windowMoveRight(int theSpeed) {
	
	if (budX < 56) {
		return false;
	}
	
	budWorldX += theSpeed;
	xWindowFine += theSpeed;				//Move fine scroll		
	
	worldX += theSpeed;	
	
	if (xWindowFine > 7) {				//Passed tile boundary?
	
		if (xLevelCoarse == 126) {		//At right edge?
			xWindowFine = 7;			//Max out fine register, we can't go further right
			worldX -= theSpeed;
			budWorldX -= theSpeed;
			return false;
		}
	
		xWindowFine -= 8;				//Subtract fine		
		
		xWindowCoarse = windowCheckRight(xWindowCoarse);
		xWindowCoarseTile = windowCheckRight(xWindowCoarseTile);
		xStripLeft = windowCheckRight(xStripLeft);
		xStripRight = windowCheckRight(xStripRight);

		xLevelStripLeft++;
		xLevelCoarse++;
		xLevelStripRight++;

		xStripDrawing = xStripRight;			//Where to draw new tiles on tilemap
		
		drawHallwayTiles(xLevelStripRight);	//Draw the strip		

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

	int temp = xWindowCoarseTile;	//Get tile position

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


int checkDoors() {
	
	uint16_t tileX = budWorldX >> 3;			//Convert Bud fine world X to tiles X

	if (budY != 104) {							//Doors only ping if Bud is on floor		
		return -1;
	}

	for (int x = 0 ; x < 6 ; x++) {
	
		if (tileX >= doorTilePos[x] && tileX <= (doorTilePos[x] + 2)) {			//Is Bud aligned with a door?
			return x;
		}
	
	}

	return -1;
	
}

int checkElevator() {
	
	uint16_t tileX = budWorldX >> 3;			//Convert Bud fine world X to tiles X

	if (budY != 104) {							//Elevator only pings if Bud is on floor		
		return -1;
	}

	if (tileX >= 71 && tileX <= 75) {			//Is Bud aligned with elevator?
		return 1;
	}

	return -1;
	
}

void drawHallway(int floor) {

	for (int x = 0 ; x < 148 ; x++) {
			populateObject(x, hallBlank, 1);		
	}
	
	populateObject(6, 3, 3);			//Left wall
	populateObject(139, 4, 3);			//Right wall

	int drawWhichDoor = 1;

	int xx = 10;

	for (int x = 0 ; x < 3 ; x++) {
		populateObject(xx, hallCounter, 4);
		//populateDoor(xx + 6, drawWhichDoor++, false);				//Stuffs the high byte with BCD floor/door number
		populateObject(xx + 12, hallCounter, 4);	

		xx += 18;
		
	}

	populateObject(64, hallCounter, 4);
	populateObject(70, hallElevator, 8);
	populateObject(78, hallCallButton, 1);
	populateObject(80, hallCounter, 4);
	
	xx = 86;

	for (int x = 0 ; x < 3 ; x++) {
		populateObject(xx, hallCounter, 4);
		//populateDoor(xx + 6, drawWhichDoor++, false);				//Stuffs the high byte with BCD floor/door number
		populateObject(xx + 12, hallCounter, 4);	

		xx += 18;
		
	}	
	
	for (int x = 0 ; x < 6 ; x++) {
	
		populateDoor(doorTilePos[x], x + 1, false);
		
	}
	

	for (int x = 150 ; x < 1000 ; x += 100) {
		thingAdd(greenie, x, 32);
	}
	
	thingAdd(chandelier, 200, 8);
	thingAdd(chandelier, 344, 8);	
	thingAdd(chandelier, 488, 8);
	thingAdd(chandelier, 664, 8);
	thingAdd(chandelier, 808, 8);
	thingAdd(chandelier, 952, 8);

	redrawCurrentHallway();
	
	//drawDecimal(234567, 0, 0);
	
	//drawText("Sloths are fun!", 0, 0, 1, false);

}

void redrawCurrentHallway() {
	
	xStripDrawing = xStripLeft;					

	for (int x = xLevelStripLeft ; x < (xLevelStripRight + 1) ; x++) {
		drawHallwayTiles(x);
		if (++xStripDrawing > 31) {
			xStripDrawing = 0;
		}
	}	
	
}

void populateDoor(uint16_t levelStrip, int whichDoorNum, bool doorState) {

	char stripCount = 0;

	int item = hallDoorClosed;
	
	if (doorState == true) {
		item = hallDoorOpen;
	}

	for (int x = 0 ; x < 4 ; x++) {
		level[levelStrip + x] = ((item << 3) | stripCount) | (currentFloor << 12) | (whichDoorNum << 8);
		stripCount++;
	}
	
}

void populateObject(uint16_t levelStrip, char whichObject, char numStrips) {

	char stripCount = 0;

	for (int x = 0 ; x < numStrips ; x++) {
		level[levelStrip + x] = (whichObject << 3) | stripCount;
		stripCount++;
	}
	
}


void drawHallwayTiles(uint16_t levelStrip) {

	char strip = level[levelStrip] & 0xFF;		//Grab low byte masking off high byte
	
	char doorNum = level[levelStrip] >> 8;		//Grab floor/door # (if it's there)

	char object = (strip & 0xF8) >> 3;		//Mask off non-object bits
	char whichStrip = strip & 0x07;	//Mask off non-strip bits
	
	switch(object) {
		
		case 0:
			hallwayBlankStrip(whichStrip);
		break;					
		case 2:
			hallwayCounterStrip(whichStrip);
		break;
		case 3:
			hallwayWallLeft(whichStrip);
		break;			
		case 4:
			hallwayWallRight(whichStrip);
		break;	
		case 5:
			hallwayElevator(whichStrip);
		break;		
		case 6:
			hallwayCallButton(whichStrip);
		break;	
		case 10:
			hallwayDoorStripClosed(whichStrip, doorNum);		//Pass the door #
		break;
		case 11:
			hallwayDoorStripOpen(whichStrip, doorNum);		//Pass the door #
		break;			
	
	}
	
}

void hallwayBlankStrip(int whichStrip) {	//Hallway object 0

	drawTile(xStripDrawing, 14, 0x9E, 3, tileBlocked);		//Draw floor as a solid blocking tile
	
	drawTile(xStripDrawing, 13, 0x9C, 3, tilePlatform);		//Draw baseboard trim

	for (int g = 0 ; g < 13 ; g++) {
		drawTile(xStripDrawing, g, 0x9D, 3);		//Draw blank walls (just as high as the doors)
	}

}

void hallwayCallButton(int whichStrip) {	//Hallway object 6

	drawTile(xStripDrawing, 14, 0x9E, 3, tileBlocked);		//Draw floor as a solid blocking tile
	
	drawTile(xStripDrawing, 13, 0x9C, 3, tilePlatform);		//Draw baseboard trim

	for (int g = 0 ; g < 13 ; g++) {
		drawTile(xStripDrawing, g, 0x9D, 3);				//Draw blank walls (just as high as the doors)
	}
	
	drawTile(xStripDrawing, 9, 0xF9, 1);		//Call button

}

void hallwayWallLeft(int whichStrip) {		//Hallway object 3

	drawTile(xStripDrawing, 14, 0x9E, 3, tileBlocked);		//Draw floor as a solid blocking tile

	switch(whichStrip) {
	
		case 0:
			for (int g = 0 ; g < 14 ; g++) {
				drawTile(xStripDrawing, g, 0xB0, 3, tileBlocked);	//Draw solid wall
			}		
		break;


		case 1:
			drawTile(xStripDrawing, 13, 0x9C, 3, tilePlatform);		//Draw baseboard trim
			for (int g = 0 ; g < 13 ; g++) {
				drawTile(xStripDrawing, g, 0x9D, 3);	//Draw blank wall
			}
			drawTile(xStripDrawing, 5, 0xA8, 1);
			drawTile(xStripDrawing, 6, 0xB8, 1);
			
			drawTile(xStripDrawing, 4, 0xA0, 3, tilePlatform);		
			
		break;
		
		case 2:
			drawTile(xStripDrawing, 13, 0x9C, 3, tilePlatform);		//Draw baseboard trim
			for (int g = 0 ; g < 13 ; g++) {
				drawTile(xStripDrawing, g, 0x9D, 3);	//Draw blank wall
			}
			drawTile(xStripDrawing, 5, 0xA9, 1);
			drawTile(xStripDrawing, 6, 0xB9, 1);
			drawTile(xStripDrawing, 4, 0xA3, 3, tilePlatform);
			
		break;
	
	}
	


}	

void hallwayWallRight(int whichStrip) {		//Hallway object 3

	drawTile(xStripDrawing, 14, 0x9E, 3, tileBlocked);		//Draw floor as a solid blocking tile

	switch(whichStrip) {
	
		case 0:
				drawTile(xStripDrawing, 13, 0x9C, 3, tilePlatform);		//Draw baseboard trim
			for (int g = 0 ; g < 13 ; g++) {
				drawTile(xStripDrawing, g, 0x9D, 3);	//Draw blank wall
			}
			drawTile(xStripDrawing, 5, 0xC8, 1, tilePlatform);
			drawTile(xStripDrawing, 6, 0xD8, 1);
			drawTile(xStripDrawing, 4, 0xA0, 3, tilePlatform);
			
		break;
		
		case 1:
			drawTile(xStripDrawing, 13, 0x9C, 3, tilePlatform);		//Draw baseboard trim
			
			for (int g = 0 ; g < 13 ; g++) {
				drawTile(xStripDrawing, g, 0x9D, 3);	//Draw blank wall
			}
			drawTile(xStripDrawing, 5, 0xC9, 1, tilePlatform);
			drawTile(xStripDrawing, 6, 0xD9, 1);
			drawTile(xStripDrawing, 4, 0xA3, 3, tilePlatform);
			
		break;
		
		case 2:
			for (int g = 0 ; g < 14 ; g++) {
				drawTile(xStripDrawing, g, 0xB1, 3, tileBlocked);	//Draw solid wall
			}		
		break;

		
	
	}
	


}	

void hallwayDoorStripClosed(int whichStrip, int whichDoorNum) {		//Hallway object 1

	const char tiles[4][9] = {
		{ 0x99, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x90, 0xA0 },
		{ 0x9A, 0x94, 0x97, 0x94, 0x94, 0x80, 0x94, 0x91, 0xA1 },	
		{ 0x9A, 0x94, 0x98, 0x94, 0x94, 0x80, 0x94, 0x91, 0xA2 },		
		{ 0x9B, 0x95, 0x95, 0x96, 0x95, 0x95, 0x95, 0x92, 0xA3 },		
	};

	drawTile(xStripDrawing, 14, 0x9E, 3, tileBlocked);		//Draw floor

	for (int g = 0 ; g < 8 ; g++) {
		drawTile(xStripDrawing, 13 - g, tiles[whichStrip][g], 0);		//Floor tile
	}
	
	setTileType(xStripDrawing, 13, tilePlatform);
	
	drawTile(xStripDrawing, 5, tiles[whichStrip][8], 3, tilePlatform);	//Draw top sil of doorway as platform

	for (int g = 0 ; g < 5 ; g++) {
		drawTile(xStripDrawing, g, 0x9D, 3);		//Blank walls
	}
	
	if (whichStrip == 1) {		
		if (apartmentState[whichDoorNum & 0x0F] & 0x80) {		//Apartment destoyed? (MSB set)
			drawTile(xStripDrawing, 8, 0x9F, 0);				//Draw X
		}
		else {
			drawTile(xStripDrawing, 8, 0x80 + (whichDoorNum >> 4), 0);		//Draw floor #
		}
	}
	if (whichStrip == 2) {
		if (apartmentState[whichDoorNum & 0x0F] == 255) {		//Apartment destoyed?
			drawTile(xStripDrawing, 8, 0x9F, 0);				//Draw X
		}
		else {
			drawTile(xStripDrawing, 8, 0x80 + (whichDoorNum & 0x0F), 0);	//Draw door #
		}			
	}	
	
}

void hallwayDoorStripOpen(int whichStrip, int whichDoorNum) {		//Hallway object 1

	const char tiles[4][9] = {
		{ 0x8E, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8A, 0xA0 },
		{ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x8B, 0xA1 },	
		{ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x8B, 0xA1 },		
		{ 0x8F, 0x8F, 0x8F, 0x8F, 0x8F, 0x8F, 0x8F, 0x8C, 0xA3 },		
	};

	drawTile(xStripDrawing, 14, 0x9E, 3, tileBlocked);		//Draw floor

	for (int g = 0 ; g < 8 ; g++) {
		drawTile(xStripDrawing, 13 - g, tiles[whichStrip][g], 0);		//Floor tile
	}
	
	setTileType(xStripDrawing, 13, tilePlatform);
	
	drawTile(xStripDrawing, 5, tiles[whichStrip][8], 3, tilePlatform);	//Draw top sil of doorway as platform

	for (int g = 0 ; g < 5 ; g++) {
		drawTile(xStripDrawing, g, 0x9D, 3);		//Blank walls
	}
		
}

void hallwayCounterStrip(int whichStrip) {	//Hallway object 2

	const char tiles[4][5] = {
		{ 0xF4, 0xE4, 0xD4, 0xC4, 0xA0 },
		{ 0xF5, 0xE5, 0xD5, 0xC5, 0xA1 },	
		{ 0xF6, 0xE6, 0xD6, 0xC6, 0xA2 },		
		{ 0xF7, 0xE7, 0xD7, 0xC7, 0xA3 },		
	};

	drawTile(xStripDrawing, 14, 0x9E, 3, tileBlocked);		//Draw floor

	for (int g = 0 ; g < 4 ; g++) {
		drawTile(xStripDrawing, 13 - g, tiles[whichStrip][g], 1);
	}	

	setTileType(xStripDrawing, 13, tilePlatform);
	
	drawTile(xStripDrawing, 9, tiles[whichStrip][4], 3, tilePlatform);	//Draw top of bureau as platform

	for (int g = 0 ; g < 9 ; g++) {
		drawTile(xStripDrawing, g, 0x9D, 3);		//Blank walls
	}
	
}

void hallwayElevator(int whichStrip) {		//Hallway object 5

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

		drawTile(xStripDrawing, 14, 0x9E, 3, tileBlocked);		//Draw floor

		for (int g = 0 ; g < 9 ; g++) {
			drawTile(xStripDrawing, 13 - g, tiles[whichStrip][g], 3);
		}
		
		drawTile(xStripDrawing, 4, tiles[whichStrip][9], 3, tilePlatform);	//Draw top sil of doorway as platform
			
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

		drawTile(xStripDrawing, 14, 0x9E, 3, tileBlocked);		//Draw floor

		for (int g = 0 ; g < 9 ; g++) {
			drawTile(xStripDrawing, 13 - g, tiles[whichStrip][g], 3);
		}		
		
		drawTile(xStripDrawing, 4, tiles[whichStrip][9], 3, tilePlatform);	//Draw top sil of doorway as platform
		
	}

	setTileType(xStripDrawing, 13, tilePlatform);	

	for (int g = 0 ; g < 3 ; g++) {
		drawTile(xStripDrawing, g, 0x9D, 3);		//Blank walls
	}
	
	if (whichStrip > 0 && whichStrip < 7) {
		drawTile(xStripDrawing, 3, floorText[whichStrip - 1], 0);
	}

	
}

void drawChandelier(int tileX) {
	
	for (int y = 0 ; y < 3 ; y++) {
		
		for (int x = 0 ; x < 4 ; x++) {	
			drawTile(tileX + x, y + 1, x, y + 13, 3);
		}
		
	}

		
		
	
	
	
}


bool thingAdd(int whichThing, int16_t x, int16_t y) {
	
	int index;
	
	for (index = 0 ; index < maxThings ; index++) {
		if (thing[index].active == false) {				//Found a slot? Break
			break;
		}
	}
	
	if (index == maxThings) {							//Didn't find a slot? Return false
		return false;
	}

	switch(whichThing) {
	
		case bigRobot:
			thing[index].active =  true;
			thing[index].state = 1;						//Active
			thing[index].type = bigRobot;
			thing[index].setPos(400, 64);
			thing[index].dir = true;					//Starts out facing left
			thing[index].setSize(3 * 8, 6 * 8);
			thing[index].turning = false;
			thing[index].subAnimate = 0;
			break;
			
		case greenie:
			thing[index].active =  true;
			thing[index].type = greenie;
			thing[index].setPos(x, y);
			thing[index].dir = true;
			thing[index].animate = 3;
			thing[index].setSize(8, 8);	
			break;	
			
		case chandelier:
			thing[index].active =  true;
			thing[index].type = chandelier;
			thing[index].setPos(x, y);
			thing[index].state = 1;	
			thing[index].setSize(4 * 8, 3 * 8);
			break;				
			
			
	}
	
	totalThings++;
	
	lastAdded = index;

	return true;
	
}

bool thingRemove(int whichThing) {

	if (thing[whichThing].active = true) {
		thing[whichThing].active = false;
		totalThings--;							//Decrement count
		lastRemoved = whichThing;
		return true;
	}		

	return false;

}

void thingLogic() {

	gpio_put(15, 1);

	for (int g = 0 ; g < maxThings ; g++) {
		if (thing[g].active) {
			
			switch(thing[g].type) {
				
				case bigRobot:
					thing[g].scan(worldX, worldY);

					switch(thing[g].state) {
						
						case 1:
							if (thing[g].turning == true) {
								if (--thing[g].animate == 0) {
									thing[g].turning = false;
								}
							}
							else {
								if (thing[g].dir == true) {				//Facing left?
									thing[g].xPos -= 2;
									if (thing[g].xPos <= 55) {
										thing[g].dir = false;			//Turn right
										thing[g].turning = true;
										thing[g].animate = 4;
									}				
								}
								else {
									thing[g].xPos += 2;
									if (thing[g].xPos >= 1104) {
										thing[g].dir = true;
										thing[g].turning = true;
										thing[g].animate = 4;
									}					
								}			
							}
							
							if (chandelierFalling == true) {				//Danger Will Robinson!
								//Only 1 can fall at once. Pass its XY wide high to the robot and see if collision
								if (thing[g].hitBox(thing[cWF].xPos, thing[cWF].yPos, thing[cWF].xPos + 32, thing[cWF].yPos + 24) == true) {
									thing[cWF].state = 3;				//Set chandelier to "falling + hit robot" so it won't generate more sound								
									thing[g].state = 9;
									thing[g].subAnimate = 0;
									thing[g].animate = 0;
									playAudio("audio/hitModem.wav");
								}									
							}					
							
						break;
						
						case 2:
						
						break;
						
					}
			
				
					break;
				
				case greenie:
					thing[g].scanG(worldX, worldY);

					if (thing[g].hitBoxSmall(budWx1, budWy1, budWx2, budWy2) == true) {
						thingRemove(g);
						playAudio("audio/greenie.wav");
					}
					
					break;	
					
				case chandelier:
					thing[g].scanC1(worldX, worldY);

					switch(thing[g].state) {
					
						case 1:											//Still installed
							if (thing[g].hitBox(budWx1, budWy1, budWx2, budWy2) == true) {
								thing[g].state = 2;						//Falling no robot hit
								chandelierFalling = true;
								cWF = g;								//Index of which one is falling
								playAudio("audio/glass_loose.wav");
							}							
							break;
						
						case 2:											//Falling 
						case 3:											//Falling after hitting robot (no SFX when land)
							thing[g].yPos += thing[g].animate;							
							if (thing[g].animate < 8) {
								thing[g].animate++;
							}
							if (thing[g].yPos > 94) {
								thing[g].yPos = 103;
								thing[g].type = chandelierSmashed;
								chandelierFalling = false;
								if (thing[g].state == 2) {				//Only play big crash if we didn't hit robot (so we don't override its sound)
									playAudio("audio/glass_2.wav");
								}
							}
							break;	
					}
					break;	
					
				case chandelierSmashed:
					thing[g].scanC2(worldX, worldY);
					//Can robot clean up?
					break;							
				
			}
		}
	}
	
	gpio_put(15, 0);
	
}




bool timer_isr(struct repeating_timer *t) {				//Every 33ms, set the flag that it's time to draw a new frame (it's possible system might miss one but that's OK)
	nextFrameFlag = true;
	return true;
}