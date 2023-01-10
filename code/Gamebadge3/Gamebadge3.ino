#include <gameBadgePico.h>
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

void setup() { //------------------------Core0 handles the file system and game logic

	gamebadge3init();

	if (loadRGB("NEStari.pal")) {
		//loadRGB("NEStari.pal");                		//Load RGB color selection table. This needs to be done before loading/change palette
		loadPalette("palette_0.dat");            	//Load palette colors from a YY-CHR file. Can be individually changed later on
		//loadPattern("moon_force.nes", 0, 512);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)

		loadPattern("patterns/basefont.nes", 0, 512);		//Load file into beginning of pattern memory and 512 tiles long (2 screens worth)

		paused = false;   							//Allow core 2 to draw once loaded
	}



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
 
  setCoarseYRollover(0, 14);   //Sets the vertical range of the tilemap, rolls back to 0 after 29

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

	//setSpriteWindow(0, 0, 119, 119); //Set window in which sprites can appear (lower 2 rows off-limits because status bar)

	drawSprite(0, 0, 0, 16, 15, 7, 4, false, false);
	
	drawSprite(0, 60, 0, 16, 15, 7, 5, false, false);
	
	drawSprite(0, 0, 1, 0, 3, false, false);
	drawSprite(8, 0, 1, 0, 3, false, true);	
	
	// if (dir == 0) {
		// if (++yPos == 239) {
		  // dir = 1;
		// }
	// }
	// else{
		// if (--yPos == 0) {
		  // dir = 0;
		// }
	// }

	setWindow(xPos, yPos);			//Set scroll window

	//setWindowSlice(30, 0); 			//Last 2 rows set no scroll
	//setWindowSlice(31, 0); 

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
	
	if (button(up_but)) {
		if (yPos > 0) {
			yPos--;
		}	
	}
	
	if (button(down_but)) {
		if (yPos < 120) {
			yPos++;
		}	
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
		//playAudio("audio/getread.wav");
		//drawText("0123456789ABC E brute?", 0, 0, true);
		//pwm_set_freq_duty(slice_num, chan, 261, 25);
		
		// Serial.print(" s-");		
		// Serial.print(pwm_gpio_to_slice_num(10), DEC);
		// Serial.print(" c-");
		// Serial.println(pwm_gpio_to_channel(10), DEC);
		pwm_set_freq_duty(6, 261, 12);
		//pwm_set_freq_duty(8, 277, 50);		
	}
	
	
	if (button(B_but)) {
		//playAudio("audio/getread.wav");
		//playAudio("audio/back.wav");
		//drawText("supercalifraguliosdosis", 0, 14, true);
		pwm_set_freq_duty(6, 261, 25);
		//pwm_set_freq_duty(8, 277, 50);
	}	
	
	if (button(C_but)) {
		//playAudio("audio/back.wav");
		//drawText("and it's been the ruin of many of a young boy. And god, I know, I'm one...", 0, 12, true);
		dutyOut = 50;
		//pwm_set_freq_duty(12, 493, 25);
		
	}	
	
	
	if (dutyOut > 0) {
		pwm_set_freq_duty(12, 100 - dutyOut, 50);
		dutyOut -= 10;
		if (dutyOut < 1) {
			dutyOut = 0;
			pwm_set_freq_duty(12, 0, 0);
		}
		
	}
	
	//gpio_put(14, 0);

}

bool timer_isr(struct repeating_timer *t) {				//Every 33ms, set the flag that it's time to draw a new frame (it's possible system might miss one but that's OK)
	nextFrameFlag = true;
	return true;
}