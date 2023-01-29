#include <gameBadgePico.h>
#include "thingObject.h"
//Your defines here------------------------------------
#include "hardware/pwm.h"

struct repeating_timer timer30Hz;			//This runs the game clock at 30Hz

bool paused = true;						//Player pause, also you can set pause=true to pause the display while loading files
bool nextFrameFlag = false;					//The 30Hz IRS sets this flag, Core0 Loop responds to it
bool drawFrameFlag = false;					//When Core0 Loop responses to nextFrameFlag, it sets this flag so Core1 will know to render a frame
bool frameDrawing = false;					//Set TRUE when Core1 is drawing the display. Core0 should wait for this to clear before accessing video memory

//Your game variables here:
int xPos = 0;
int yPos = 0;
int dir = 0;



int dutyOut;

enum movement { rest, starting, walking, running, jumping, moving, turning, stopping };

movement budState = rest;

int budFrame = 0;
int budSubFrame = 0;
bool budDir = false;	//True = going left
int budX = 56;
int budY = 8;

uint16_t budWorldX = 8 * (6 + 7);					//Where the camera is positioned in the world (not the same as tilemap as that's dynamic)
uint16_t budWorldY = 8;

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

#define hallDoor		1
#define hallBlank		0
#define hallCounter		2
#define hallLeftWall	3
#define hallRightWall	4
#define hallElevator	5
#define hallCallButton	6

#define bigRobot		1
#define chandelier		2
#define roomba			3
#define greenie			255

int currentFloor = 1;

int apartmentState[7] = { 0, 0, 255, 0, 0, 0, 0 };	//Zero index not used, starts at 1 goes to 6

bool elevatorOpen = false;

#define maxThings		64

thingObject thing[maxThings];

int numberThings = 0;

void setup() { //------------------------Core0 handles the file system and game logic

	gamebadge3init();

	if (loadRGB("NEStari.pal")) {
		//loadRGB("NEStari.pal");                		//Load RGB color selection table. This needs to be done before loading/change palette
		//loadPalette("palette_0.dat");            	//Load palette colors from a YY-CHR file. Can be individually changed later on
		//loadPattern("moon_force.nes", 0, 512);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)

		loadPalette("bud.dat");            	//Load palette colors from a YY-CHR file. Can be individually changed later on
		loadPattern("bud.nes", 0, 1024);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)

		//loadPattern("patterns/basefont.nes", 0, 512);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)

		paused = false;   							//Allow core 2 to draw once loaded
	}

	//setButtonDebounce(B_but, false, 0);
	
	setCoarseYRollover(0, 14);   //Sets the vertical range of the tilemap, rolls back to 0 after 29
	
	drawHallway(1);

}

void setup1() { //-----------------------Core 1 handles the graphics stuff
  
  while (paused == true) {
	  delayMicroseconds(1);
  }

  int tileG = 0;
  
  // for (int y = 0 ; y < 16 ; y++) {  
  
    // for (int x = 0 ; x < 16 ; x++) {  
      // drawTile(x, y, tileG++ & 0xFF, y & 0x03);
    // } 
     // for (int x = 16 ; x < 32 ; x++) {  
      // drawTile(x, y, 'A', 0);
    // }
     
  // } 
  
  // tileG = 0;
  
   // for (int y = 16 ; y < 32 ; y++) {  
  
    // for (int x = 0 ; x < 16 ; x++) {  
      // drawTile(x, y, tileG++ & 0xFF, y & 0x03);
    // } 
     // for (int x = 16 ; x < 32 ; x++) {  
      // drawTile(x, y, 'B', 0);
    // }
     
  // }  
  

  // for (int x = 0 ; x < 15 ; x++) {  
    // drawTile(x, 30, 10, 0x03);
  // } 
  // for (int x = 0 ; x < 15 ; x++) {  
    // drawTile(x, 31, 15, 0x03);
  // } 
 
  //fillTiles(0, 0, 31, 31, 0, 0);
  
  // for (int x = 0 ; x < 32 ; x += 2) {
	  // drawTile(x, 13, 2, 0, 3);
	  // drawTile(x + 1, 13, 3, 0, 3);
	  // drawTile(x, 14, 2, 1, 3);
	  // drawTile(x + 1, 14, 3, 1, 3);	  
  // }

  //setCoarseYRollover(0, 14);   //Sets the vertical range of the tilemap, rolls back to 0 after 29

  //setWinYjump(0x80, 13, 30);	//On row 13, jump to tilemap row 30 (such as for a status window)
  //setWinYjump(0x80, 14, 31); 	//On row 14, jump to tilemap row 31. Both of these rows will be set to "no scroll"

  add_repeating_timer_ms(-33, timer_isr, NULL, &timer30Hz);

}

void loop() {	//-----------------------Core 0 handles the main logic loop
	
	if (nextFrameFlag == true) {		//Flag from the 30Hz ISR?
		nextFrameFlag = false;			//Clear that flag		
		drawFrameFlag = true;			//and set the flag that tells Core1 to draw the LCD
		gameFrame();					//Now do our game logic in Core0
		serviceDebounce();				//Debounce buttons
	}

	if (paused == false) {   
		if (button(start_but)) {      //Button pressed?
		  paused = true;
		  Serial.println("PAUSED");
		}      
	}
	else {
		if (button(start_but)) {      //Button pressed?
		  paused = false;
		  Serial.println("UNPAUSED");
		}    
	}

  
}

void loop1() { //------------------------Core 1 handles the graphics blitter. Core 0 can process non-graphics game logic while a screen is being drawn

	if (drawFrameFlag == true) {			//Flag set to draw display?

		drawFrameFlag = false;				//Clear flag
		
		if (paused == true) {				//If system paused, return (no draw)
			return;
		}		

		frameDrawing = true;				//Set flag that we're drawing a frame. Core0 should avoid accessing video memory if this flag is TRUE
		sendFrame();
		frameDrawing = false;				//Clear flag. Core0 is now free to access screen memory (I guess this core could as well?)

	}

}

void gameFrame() { //--------------------This is called at 30Hz. Your main game state machine should reside here

	if (paused == true) {
		return;
	}
	
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

	bool animateBud = false;			//Bud is animated "on twos" (every other frame at 30HZ, thus Bud animates at 15Hz)
	if (++budSubFrame > 1) {
		budSubFrame = 0;
		animateBud = true;
	}		

	int speed = 1;
	int budFrameLimit = 7;
	int jumpGFXoffset = 6;						//Jumping up	
			
	switch(budState) {
	
		case rest:
			if (animateBud) {
				if (++tail > 2) {
					tail = 0;
				}
				if (++blink > 40) {
					blink = 0;
				}
			}
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
			break;
		
		case moving:							
			budFrameLimit = 6;
			if (budDir == false) {		
				drawSprite(budX, budY - 8, 0, 16 + (budFrame << 1), 3, 2, 4, budDir, false);	//Running
				budMoveRight(3);		
			}
			else {				
				drawSprite(budX - 8, budY - 8, 0, 16 + (budFrame << 1), 3, 2, 4, budDir, false);	//Running
				budMoveLeft(3);		
			}
			if (animateBud) {
				if (++budFrame > budFrameLimit) {
					budFrame = 0;
				}					
			}			
			break;	

		case jumping:
			if (jump == 128) {
				jumpGFXoffset = 8;						//Falling down
			}
		
			if (budDir == false) {	
				drawSprite(budX, budY - 8, 0, 16 + jumpGFXoffset, 3, 2, 4, budDir, false);
				if (moveJump == true) {
					budMoveRight(3);	
				}				
			}
			else {		
				drawSprite(budX - 8, budY - 8, 0, 16 + jumpGFXoffset, 3, 2, 4, budDir, false);
				if (moveJump == true) {
					budMoveLeft(3);	
				}		
			}				

			break;

		case stopping:
			drawSprite(budX, budY - 8, 0, 16 + (budFrame << 1), 3, 2, 4, budDir, false);	
			if (animateBud) {
				if (budFrame == 0) {
					budState = rest;
				}								
				if (++budFrame > 6) {
					budFrame = 0;
				}					
			}	
			if (budDir == false) {
				xPos += 1;			
				if (xPos > 255) {
					xPos -= 256;
				}					
			}
			else {
				xPos -= 1;			
				if (xPos < 0) {
					xPos += 256;
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
				budState = jumping;
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

	
	thingLogic();
	

	
	setWindow((xWindowCoarse << 3) | xWindowFine, yPos);			//Set scroll window

	// setWindowSlice(0, xPos << 1); 			//Last 2 rows set no scroll
	// setWindowSlice(1, xPos << 1); 			//Last 2 rows set no scroll	
	// setWindowSlice(2, xPos << 1); 			//Last 2 rows set no scroll
	// setWindowSlice(3, xPos << 1); 			//Last 2 rows set no scroll		
	//setWindowSlice(14, xPos << 1); 

	bool budNoMove = true;
	moveJump = false;

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
	
	if (button(C_but)) {
		//playAudio("audio/back.wav");
		//drawText("and it's been the ruin of many of a young boy. And god, I know, I'm one...", 0, 12, true);
		//dutyOut = 50;
		//pwm_set_freq_duty(12, 493, 25);
		
		if (jump == 0) {
			jump = 1;
			velocikitten = 9;
			budNoMove = false;
			budState = jumping;	
		}
		
	}		
	
	if (budNoMove == true && budState == moving) {		//Was Bud moving, but then d-pad released? Bud comes to a stop
		budState = stopping;
		budFrame = 0;
	}
		
    uint slice_num = pwm_gpio_to_slice_num(14);
    uint chan = pwm_gpio_to_channel(14);
	
	if (button(select_but)) {

	  pwm_set_freq_duty(6, 0, 0);	  
	  pwm_set_freq_duty(8, 0, 0);
	  pwm_set_freq_duty(10, 0, 0);
	  pwm_set_freq_duty(12, 0, 0);
	  fillTiles(0, 0, 31, 31, ' ', 0);
	  
	}



	if (button(A_but)) {
		// budY = 15;
		// velocikitten = 1;
		// jump = 255;
		// playAudio("audio/getread.wav");
		
		
		thingRemove(0xFF);
		
		//drawText("0123456789ABC E brute?", 0, 0, true);
		//pwm_set_freq_duty(slice_num, chan, 261, 25);
		
		// Serial.print(" s-");		
		// Serial.print(pwm_gpio_to_slice_num(10), DEC);
		// Serial.print(" c-");
		// Serial.println(pwm_gpio_to_channel(10), DEC);
		//pwm_set_freq_duty(6, 261, 12);
		//pwm_set_freq_duty(8, 277, 50);		
	}
	
	
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
	
	drawSpriteDecimal(budWorldX, 60, 0, 0);
	drawSpriteDecimal(budWorldY, 50, 8, 0);	
	drawSpriteDecimal(numberThings, 40, 16, 0);
	drawSpriteDecimal(worldX, 30, 24, 0);	
	
	// drawSpriteDecimal(budX, 8, 0, 0);		
	// drawSpriteDecimal(xWindowBudFine, 8, 8, 0);	
	// drawSpriteDecimal(xWindowCoarseTile, 8, 16, 0);
	// drawSpriteDecimal(xLevelStripRight, 8, 48, 0);		


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
	
	if (budX < 64) {
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
		populateDoor(xx + 6, drawWhichDoor++);				//Stuffs the high byte with BCD floor/door number
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
		populateDoor(xx + 6, drawWhichDoor++);				//Stuffs the high byte with BCD floor/door number
		populateObject(xx + 12, hallCounter, 4);	

		xx += 18;
		
	}	

	for (int x = 100 ; x < 1000 ; x += 75) {
		thingAdd(greenie, x, 32);
	}


	xStripDrawing = xStripLeft;
	
	for (int x = xLevelStripLeft ; x < (xLevelStripRight + 1) ; x++) {
		drawHallwayTiles(x);
		xStripDrawing++;	
	}
	
	// drawDecimal(234567, 0, 0);
	
	//drawText("Sloths are fun!", 0, 0, 1, false);

}

void populateDoor(uint16_t levelStrip, int whichDoorNum) {

	char stripCount = 0;

	for (int x = 0 ; x < 4 ; x++) {
		level[levelStrip + x] = ((hallDoor << 3) | stripCount) | (currentFloor << 12) | (whichDoorNum << 8);
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
		case 1:
			hallwayDoorStrip(whichStrip, doorNum);		//Pass the door #
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

void hallwayDoorStrip(int whichStrip, int whichDoorNum) {		//Hallway object 1

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
		if (apartmentState[whichDoorNum & 0x0F] == 255) {		//Apartment destoyed?
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
			{ 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xA1 },	
			{ 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xA1 },	
			{ 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xA1 },	
			{ 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xA1 },	
			{ 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xA1 },	
			{ 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xA1 },	
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
	
	if (numberThings == maxThings) {
		return false;
	}

	switch(whichThing) {
	
		case bigRobot:
			thing[numberThings].active =  true;
			thing[numberThings].type = bigRobot;
			thing[numberThings].setPos(1104, 64);
			thing[numberThings].dir = 0;		
			break;
			
		case greenie:
			thing[numberThings].active =  true;
			thing[numberThings].type = greenie;
			thing[numberThings].setPos(x, y);
			thing[numberThings].dir = 0;		
			break;	
			
	}
	
	numberThings++;

	return true;
	
}

bool thingRemove(int whichThing) {

	if (whichThing == 0xFF) {							//No param? Pop last thing
		if (numberThings > 0) {
			numberThings--;							//Dec down. Next thing added goes in this slot
			thing[numberThings].active = false;		//Make inactive (vars still there since no dynamic mem)
			return true;
		}
	}
	else {
		if (thing[numberThings].active = true) {
			thing[numberThings].active = false;
			return true;
		}		
	}
	
	return false;

}

void thingLogic() {

	gpio_put(15, 1);

	if (numberThings > 0) {
		for (int g = 0 ; g < (numberThings) ; g++) {
			if (thing[g].active) {
				
				switch(thing[g].type) {
					
					case bigRobot:
						thing[g].scan(worldX, worldY);
						if (thing[g].dir == 0) {
							thing[g].xPos -= 2;
							if (thing[g].xPos <= 55) {
								thing[g].dir = 1;
							}				
						}
						else {
							thing[g].xPos += 2;
							if (thing[g].xPos >= 1104) {
								thing[g].dir = 0;
							}					
						}						
						
						break;
					
					case greenie:
						thing[g].scanG(worldX, worldY);

						if (thing[g].hitBox(budWorldX, budWorldY - 16, budWorldX + 24, budWorldY) == true) {
							thing[g].active = false;
							playAudio("audio/greenie.wav");
						}
						
						break;					
					
				}
			}
		}
		
	}	
	
	gpio_put(15, 0);
	
}




bool timer_isr(struct repeating_timer *t) {				//Every 33ms, set the flag that it's time to draw a new frame (it's possible system might miss one but that's OK)
	nextFrameFlag = true;
	return true;
}