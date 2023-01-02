#include <gameBadgePico.h>

struct repeating_timer timer30Hz;			//This runs the game clock at 30Hz

bool paused = false;						//Player pause, also you can set pause=true to pause the display while loading files
bool nextFrameFlag = false;					//The 30Hz IRS sets this flag, Core0 Loop responds to it
bool drawFrameFlag = false;					//When Core0 Loop responses to nextFrameFlag, it sets this flag so Core1 will know to render a frame
bool frameDrawing = false;					//Set TRUE when Core1 is drawing the display. Core0 should wait for this to clear before accessing video memory

//Your game variables here:
int xPos = 0;
int yPos = 0;
int dir = 0;

int debounce = 0;

void setup() { //------------------------Core0 handles the file system and game logic

	paused = true;                      		//Prevent 2nd core from drawing the screen until file system is booted

	gamebadge3init();

	if (loadRGB("NEStari.pal")) {
		//loadRGB("NEStari.pal");                		//Load RGB color selection table. This needs to be done before loading/change palette
		loadPalette("palette_0.dat");            	//Load palette colors from a YY-CHR file. Can be individually changed later on
		loadPattern("moon_force.nes", 0, 512);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)

		paused = false;   							//Allow core 2 to draw once loaded
	}

}

void setup1() { //-----------------------Core 1 handles the graphics stuff
  
  int tileG = 0;
  
  for (int y = 0 ; y < 32 ; y++) {  
  
    for (int x = 0 ; x < 16 ; x++) {  
      drawTile(x, y, tileG++ & 0xFF, y & 0x03);
    } 
     for (int x = 16 ; x < 32 ; x++) {  
      drawTile(x, y, ' ', 0);
    }
     
  } 

  for (int x = 0 ; x < 15 ; x++) {  
    drawTile(x, 30, 27, 0x03);
  } 
  for (int x = 0 ; x < 15 ; x++) {  
    drawTile(x, 31, 0, 0x03);
  } 

  clearSprite();

  setCoarseYRollover(0, 29);   //Sets the vertical range of the tilemap, rolls back to 0 after 29

  setWinYJump(0x80, 13, 30);
  setWinYJump(0x80, 14, 31); 

  add_repeating_timer_ms(33, timer_isr, NULL, &timer30Hz);


}

void loop() {	//-----------------------Core 0 handles the main logic loop
	
	if (nextFrameFlag == true) {		//Flag from the 30Hz ISR?
		nextFrameFlag = false;			//Clear that flag		
		drawFrameFlag = true;			//and set the flag that tells Core1 to draw the LCD
		gameFrame();					//Now do our game logic in Core0
	}

	if (paused == false) {   
		if (button(start_but)) {      //Button pressed?
		  paused = true;
		  Serial.println("PAUSED");
		  while(button(start_but)) {
			delay(5);
		  }   
		}      
	}
	else {
		if (button(start_but)) {      //Button pressed?
		  paused = false;
		  Serial.println("UNPAUSED");
		  while(button(start_but)) {
			delay(5);
		  }   
		}    
	}

  
}

void loop1() { //------------------------Core 1 handles the graphics blitter. Core 0 can process non-graphics game logic while a screen is being drawn

	if (drawFrameFlag == true) {			//Flag set to draw display?

		drawFrameFlag = false;				//Clear flag
		
		if (paused == true) {				//If system paused, return (no draw)
			return;
		}		

		//gpio_put(12, 1);					//Scope test timing

		//gpio_put(13, 1);					//Scope test timing
		frameDrawing = true;				//Set flag that we're drawing a frame. Core0 should avoid accessing video memory if this flag is TRUE
		sendFrame();
		frameDrawing = false;				//Clear flag. Core0 is now free to access screen memory (I guess this core could as well?)
		//gpio_put(13, 0);					//Scope test timing

		//gpio_put(12, 10);					//Scope test timing

	}

}

void gameFrame() { //--------------------This is called at 30Hz. Your main game state machine should reside here

	if (paused == true) {
		return;
	}
	
	while(frameDrawing == false) {			//Wait for Core1 to begin rendering the frame
		delayMicroseconds(1);				//Do nothing (arduino doesn't like empty while()s
	}
	//OK we now know that Core1 has started drawing a frame. We can now do any game logic that doesn't involve accessing video memory
	
	serviceAudio();
	
	//Controls, file access, etc...
	
	while(frameDrawing == true) {			//OK we're done with our non-video logic, so now we wait for Core 1 to finish drawing
		delayMicroseconds(1);
	}
	// while(isDMAbusy(0) == true) {			//After Core1 clears the frameDrawing flag the final DMA will likely still be processing, so wait for that to end as well (probably not needed as the buffer will already have been built)
		// delayMicroseconds(1);
	// }
	
	//OK now we can access video memory. We have about 15ms in which to do this before the next frame starts--------------------------

	//gpio_put(14, 1);

	setSpriteWindow(0, 0, 119, 103); //Set window in which sprites can appear (lower 2 rows off-limits because status bar)

	drawSprite(xPos >> 2, xPos, 0, 0, 8, 8, 0);

	if (dir == 0) {
		if (++yPos == 100) {
		  dir = 1;
		}
	}
	else{
		if (--yPos == 16) {
		  dir = 0;
		}
	}

	setWindow(xPos, yPos);

	setWindowSlice(0, 0); 
	setWindowSlice(1, 0); 

	setWindowSlice(30, 0); 
	setWindowSlice(31, 0); 

	if (button(left_but)) {
		xPos--;
		if (xPos < 0) {
			xPos += 256;
		}		
	}
	
	if (button(right_but)) {
		xPos++;
		if (xPos > 255) {
			xPos -= 256;
		}		
	}

	if (button(A_but)) {
		playAudio("audio/getread.wav");
	}
	
	
	if (button(B_but)) {
		playAudio("audio/back.wav");
	}	
	
	if (button(C_but)) {
		playAudio("audio/back2.wav");
	}	
	
	// xPos += 1;
	// if (xPos > 255) {
		// xPos -= 256;
	// }

	//gpio_put(14, 0);
	
}


bool timer_isr(struct repeating_timer *t) {				//Every 33ms, set the flag that it's time to draw a new frame (it's possible system might miss one but that's OK)
	nextFrameFlag = true;
	return true;
}

