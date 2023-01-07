//Game & graphics driver for gameBadgePico (MGC 2023)

#include <hardware/pio.h>								//PIO hardware
#include "audio.pio.h"									//Our assembled program
PIO pio = pio0;
uint programOffset;

#include "Arduino.h"
#include "pico_ST7789.h"
#include "gameBadgePico.h"

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"

#include "SPI.h"
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_TinyUSB.h"

Adafruit_FlashTransport_RP2040 flashTransport;		// RP2040 use same flash device that store code.	// Therefore there is no need to specify the SPI and SS
Adafruit_SPIFlash flash(&flashTransport);			// Use default (no-args) constructor to be compatible with CircuitPython partition scheme
FatFileSystem fatfs;                        		// file system object from SdFat
FatFile file;										// Object to open files
Adafruit_USBD_MSC usb_msc;                  		// USB Mass Storage object

#define LCD_WIDTH  240
#define LCD_HEIGHT 240

static uint16_t linebuffer[2][3840];    //Sets up 2 buffers of 120 longs x 16 lines high
				//--------Y---X--- We put the X value second so we can use a pointer to rake the data
static uint16_t nameTable[32][32];		//4 screens of tile data. Can scroll around it NES-style (LCD is 15x15, tile is 16X16, slightly larger) X is 8 bytes wider to hold the palette reference (similar to NES but with 1 cell granularity)
static uint16_t patternTable[8192];		//256 char patterns X 8 lines each, 16 bits per line. Same size as NES but chunky pixel, not bitplane and stored in shorts
static uint16_t spriteBuffer[14400];
uint16_t paletteRGB[32];				//Stores the 32 colors as RGB values, pulled from nesPaletteRGBtable
uint16_t nesPaletteRGBtable[64];		//Stores the current NES palette in 16 bit RGB format

uint16_t baseASCII = 32;				//Stores what tile in the pattern table is the start of printable ASCII (space, !, ", etc...) User can change the starting position to put ASCII whereever they want in pattern table, but this is the default
uint8_t textWrapEdges[2] = {0, 14};		//Sets a left and right edge where text wraps in the tilemap. Use can change this, default is left side of scroll, one screen wide
bool textWordWrap = true;				//Is spacing word-wrap enabled (fancy!)

uint8_t winX[32];
uint8_t winXfine[32];
uint8_t winY = 0;
uint8_t winYfine = 0;
uint8_t winYJumpList[16];				//One entry per screen row. If flag set, winY coarse jumps to nametable row specified and yfinescroll is reset to 0
uint8_t winYreset = 0; 					//What coarse Y rollovers to in nametable when it reaches winYrollover
uint8_t winYrollover = 31;				//If coarse window Y exceeds this, reset to winYreset. Use this drawing status bars or creating vertical margins for word wrap
int xLeft = -1;							//Sprite window limits. Use this to prevent sprites from drawing over status bar (reduce yBottom by 12)
int xRight = 120;
int yTop = -1;
int yBottom = 120;

uint32_t audioSamples = 0;
uint8_t whichAudioBuffer = 0;
bool audioPlaying = false;

#define audioBufferSize	512
uint32_t audioBuffer0[audioBufferSize] __attribute__((aligned(audioBufferSize)));
uint32_t audioBuffer1[audioBufferSize] __attribute__((aligned(audioBufferSize)));

bool fillBuffer0flag = false;
bool fillBuffer1flag = false;
bool endAudioBuffer0flag = false;
bool endAudioBuffer1flag = false;
int audio_pin_slice;

int indexToGPIO[9] = {6, 8, 7, 9, 21, 5, 4, 3, 2};		//Button index is UDLR SEL STR A B C  this maps that to the matching GPIO
bool debounce[9] = {false, false, false, false, true, true, true, true, true};		//Index of which buttons have debounce (button must open before it can re-trigger)
uint8_t debounceStart[9] = {0, 0, 0, 0, 5, 5, 1, 1, 1};								//If debounce, how many frames must button be open before it can re-trigger.
uint8_t debounceTimer[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};    							//The debounceStart time is copied here, and debounceTimer is what's decrimented

const struct st7789_config lcd_config = {
    .spi      = PICO_DEFAULT_SPI_INSTANCE,
    .gpio_din = PICO_DEFAULT_SPI_TX_PIN,
    .gpio_clk = PICO_DEFAULT_SPI_SCK_PIN,
    .gpio_cs  = PICO_DEFAULT_SPI_CSN_PIN,
    .gpio_dc  = 20,
    .gpio_rst = 21,
    .gpio_bl  = 22
};

void gamebadge3init() {				//Instantiate class and set stuff up

	flash.begin();

	usb_msc.setID("Adafruit", "External Flash", "1.0");                     // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
	usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);  // Set callback
	usb_msc.setCapacity(flash.size()/512, 512);                             // Set disk size, block size should be 512 regardless of spi flash page size
	usb_msc.setUnitReady(true);                                             // MSC is ready for read/write
	usb_msc.begin();

	Serial.begin(115200);

	if (!fatfs.begin(&flash)) {
		Serial.println("Failed to init files system, flash may need to be formatted");
	}

	Serial.println("Adafruit TinyUSB Mass Storage External Flash example");
	Serial.print("JEDEC ID: 0x"); Serial.println(flash.getJEDECID(), HEX);
	Serial.print("Flash size: "); Serial.print(flash.size() / 1024); Serial.println(" KB");

	st7789_init(&lcd_config, 240, 240);			//Set up LCD for great justice

	st7789_setRotation(1);						//Ribbon cable on left side of display
	
	gpio_init(14);
	gpio_set_dir(14, GPIO_OUT);
	
	gpio_init(15);
	gpio_set_dir(15, GPIO_OUT);
	gpio_put(15, 0);
	
	gpio_set_function(10, GPIO_FUNC_PWM);
	audio_pin_slice = pwm_gpio_to_slice_num(10);	
	pwm_config config = pwm_get_default_config();
	
	pwm_config_set_clkdiv(&config, 5.5f); 
    pwm_config_set_wrap(&config, 255); 
    pwm_init(audio_pin_slice, &config, true);

    pwm_set_gpio_level(10, 0);

	for (int x = 0 ; x < 9 ; x++) {				//Setup the 9 GPIO used for controls
		setGPIObutton(indexToGPIO[x]);									//Init each pin using the index mapping
		setButtonDebounce(x, debounce[x], debounceStart[x]);			//Pass the default values back into themselves. User can change these later on
		//For instance menus, you'll want to set a debounce for the d-pad so it's one click per action. Then disable d-pad debounce when game starts	
	}

	//programOffset = pio_add_program(pio, &audio_program);
	//audio_program_init(pio, 0, programOffset, 10);
		
}

void setGPIObutton(int which) {
	gpio_init(which);
	gpio_set_dir(which, GPIO_IN);
	gpio_pull_up(which);			
}

void setButtonDebounce(int which, bool useDebounce, uint8_t frames) {	

	debounce[which] = useDebounce;
	
	if (useDebounce == true && frames < 1) {			//If debounce you must have at minimum 1 frame of debounce time (at least one frame off before can retrigger)
		frames = 1;
	}

	debounceStart[which] = frames;	
}

bool button(int which) {				//A, B, C, select, start, up, down, left, right

	if (debounce[which] == true) {								//Switch has debounce?
		if (debounceTimer[which] == 0) {						//Has timer cleared?
			if (gpio_get(indexToGPIO[which]) == 0) {			//OK now you can check for a new button press
				debounceTimer[which] = debounceStart[which];		//Yes? Set new timer
				return true;										//and return button pressed
			}				
		}
	}
	else {
		if (gpio_get(indexToGPIO[which]) == 0) {		//Button pressed? That's all we care about
			return true;
		}		
	}

	return false;
	
}

void serviceDebounce() {				//Must be called every frame, even if paused (because the buttons need to work to unpause!)

	for (int x = 0 ; x < 9 ; x++) {				//Scan all buttons
		if (debounce[x] == true) {				//Button uses debounce?
			if (debounceTimer[x] > 0) {			//Is the value above 0? That means button was pressed in the past
				if (gpio_get(indexToGPIO[x]) == 1) {			//Has button opened?
					debounceTimer[x]--;			//If so, decrement debounce while it's open. When debounce reaches 0, it can be re-triggered under button funtion
				}		
			}
		}		
	}
	
}

bool loadRGB(const char* path) {

	if (file.open(path, O_RDONLY)) {
		
	  for (int x = 0 ; x < 64 ; x++) {
		updatePaletteRGB(x, file.read(), file.read(), file.read()); 
	  }

	  file.close();	
	  
	  return true;
	
	}
	
	return false;

}

void loadPalette(const char* path) {

  file.open("palette_0.dat", O_RDONLY);

  for (int x = 0 ; x < 32 ; x++) {
    updatePalette(x, file.read()); 
  }

  file.close();
  
}

void loadPattern(const char* path, uint16_t start, uint16_t length) {	//Loads a pattern file into memory. Use start & length to only change part of the pattern, like of like bank switching!

  unsigned char lowBit[8];
  unsigned char highBit[8];

  digitalWrite(LED_BUILTIN, HIGH);

  file.open(path, O_RDONLY);

  for (uint16_t numChar = start ; numChar < (start + length) ; numChar++) {		//Read a bitplane filestream created by YY-CHR

    file.read(lowBit, 8);				//Stream the low bits into array
    file.read(highBit, 8);				//Stream the high bits into array	

    convertBitplanePattern(numChar << 3, lowBit, highBit);		//Pass array points to function that will convert to chunky pixels
    
  }

  file.close();

  digitalWrite(LED_BUILTIN, LOW);
  
}


//In tilemap position XY, draw the tile at index "whattile" from the pattern table using whatPalette (0-7)
void drawTile(int xPos, int yPos, uint16_t whatTile, char whatPalette) {

		nameTable[yPos][xPos] = (whatPalette << 10) | whatTile;			//Palette points to a grouping of 4 colors, so we shift the palette # 2 to the left to save math during render		
}

//In tilemap position XY, draw the tile at position tileX to the right, tileY down, from the pattern table using whatPalette (0-7)
void drawTile(int xPos, int yPos, uint16_t tileX, uint16_t tileY, char whatPalette) {

		uint16_t whatTile = (tileY * 16) + tileX;						//Do this math for the user :)

		nameTable[yPos][xPos] = (whatPalette << 10) | whatTile;			//Palette points to a grouping of 4 colors, so we shift the palette # 2 to the left to save math during render		
}

//By default draw decimal/text assumes printable ASCII starts at pattern tile 32 (space) Use this function if you put ASCII elsewhere in your pattern table
void setASCIIbase(uint16_t whatBase) {
	
	baseASCII = whatBase;

}

//If drawing text with doWrap = true, this sets the left and right margins in the name table
void setTextMargins(int left, int right) {
	
	textWrapEdges[0] = left;
	textWrapEdges[1] = right;
	
	//If you want stuff to wrap within a static screen, you'd use 0, 14 (the default)
	//If you want to fill the name table with text, such as frame a webpage, you could set 0, 31 and then scroll left and right to read all text
	//Text carriage returns wrap within the same confines defined by setCoarseYRollover(int topRow, int bottomRow);
	//Text that goes past bottomRow will wrap to topRow
	
}

//Draws a string of horizontal text at position x,y in the tilemap
void drawText(const char *text, uint8_t x, uint8_t y, bool doWrap = false) {

	if (doWrap) {

		bool cr = false;								//Set TRUE to perform a carriage return (several things can cause this hence the flag)

		while(*text) {
			if (*text == ' ') {							//If current character is a space, then we need to do special checks
				//Serial.print("Space found. ");
				if (x == textWrapEdges[0]) {			//No reason to draw spaces on left margin
					//Serial.println("Left margin skip");
					text++;								//Advance pointer but not X
				}
				else {												//Not on left margin?
					if (x == textWrapEdges[1]) {					//Does this space land on the right margin?
						//Serial.println("On right margin. Draw space and do CR");
						nameTable[y][x] = *text++;					//Draw the space...
						cr = true;									//and set carriage return for next char
					}
					else {											//OK so there is at least 1 character of space after this space " "
						//Serial.print("Next word='");
						int charCount = 0;								//Let's count the characters in the next word
						nameTable[y][x++] = *text;					    //Draw the space and advance x

						while(*text == 32) {							//Move text past the space and keep going until we find a non-space
							*text++;
						}
						
						if (*text == 0) {								//Ok we're past all the damn spaces. Did we hit end of string because some asshole typed "hello world!   " ?
							return;										//Fuck it we're done!
						}
			
						while(*text != 32 && *text != 0) {				//Look for the next space or end of string
							//Serial.write(*text);						//Debug
							charCount++;								//Count how many chars we find in front of next space or end
							text += 1;									//Advance the pointer
						}
						
						//Serial.print("' length = ");
						//Serial.print(charCount, DEC);
						//Serial.print(" WRAP? ");
						text -= charCount;								//Rollback pointer to start of word because we still need to draw it
						if (charCount > ((textWrapEdges[1] + 1) - x)) {		//Check if enough characters to draw this word (offset +1 because we need count not index)
							//Serial.println("YES");						//Have to make sure stuff like " to" or " I" renders
							cr = true;
						}
						else {
							//Serial.println("NO");
						}					
					}	
				}
			}
			else {
				if (x == textWrapEdges[1]) {		//Will this character land on the right margin?
					text++;							//Check next character
					
					if (*text == ' ') {				//Single character with a space after? "a ", " I"...
						text--;						//Backup pointer
						nameTable[y][x] = *text++;	//and print it, advancing the pointer again. Next line will see the space and skip it since beginning of line
					}
					else {
						nameTable[y][x] = '-';		//Draw a hyphen
						text--;						//Backup pointer so the character that wasn't ' ' will print on next line
					}
					cr = true;
				}
				else {
					nameTable[y][x++] = *text++;	//Standard draw
				}			
			}		
			
			if (cr == true) {						//Flag to do carriage return?
				//Serial.println("Carriage Return");
				cr = false;							//Clear flag
				x = textWrapEdges[0];				//Jump to left margin
				if (++y > winYrollover) {			//Advance Y row and rollover if it goes past bottom limit
					y = winYreset;
				}
			}	
			//Serial.print(x, DEC);
			//Serial.print(" ");
			//Serial.println(y, DEC);
		}
	}
	else {
		while(*text) {
			nameTable[y][x++] = *text++;			//Just draw the characters from left to write
		}
	}

}


void drawSprite(int xPos, int yPos, uint16_t tileX, uint16_t tileY, uint16_t xWide, uint16_t yHigh, uint8_t whichPalette) {

	for (int yTiles = tileY ; yTiles < (tileY + yHigh) ; yTiles++) {	

		int xPosTemp = xPos;
	
		for (int xTiles = tileX ; xTiles < (tileX + xWide) ; xTiles++) {
			drawSprite(xPosTemp, yPos, xTiles, yTiles, whichPalette);	
			xPosTemp += 8;
		}		
		yPos += 8;	
		
	}
	
}

void drawSprite(int xPos, int yPos, uint16_t tileX, uint16_t tileY, uint8_t whichPalette) {
	
	//gpio_put(13, 1);

	tileY += 16;

	tileX <<= 3;
	tileY <<= 7;
	
	uint16_t *sp = &spriteBuffer[(yPos * 120) + xPos];    				  //Use pointer so less math later on

	uint16_t *tilePointer = &patternTable[tileX + tileY];

	for (int yPixel = yPos ; yPixel < (yPos + 8) ; yPixel++) {

		uint16_t temp = *tilePointer;						//Get pointer for sprite tile

		for (int xPixel = xPos ; xPixel < (xPos + 8) ; xPixel++) {
			
			uint8_t twoBits = temp >> 14;					//Smash this down into 2 lowest bits
			
			//For a sprite pixel to be drawn, it must be within the current window, not a 0 index color, and not over an existing sprite pixel
			if (xPixel > xLeft && xPixel < xRight && yPixel > yTop && yPixel < yBottom && twoBits != 0 && *sp == 0xF81f) {
				*sp = paletteRGB[(whichPalette << 2) + twoBits];
			}
			sp++;
			temp <<= 2;										//Shift for next 2 bit color pair

		}
		
		tilePointer++;
		
		sp += (120 - 8);
		
	}
	
	//gpio_put(13, 0);

}	

void clearSprite() {
	
	for (int x = 0 ; x < 14400 ; x++) {   
		spriteBuffer[x] = 0xF81F;
	} 
	
}

void updatePattern(uint16_t position, char lowByte, char highByte) {			//Allows game code to load patterns off flash/SD
	
	patternTable[position] = lowByte | (highByte << 8);	
	
}

void convertBitplanePattern(uint16_t position, unsigned char *lowBitP, unsigned char *highBitP) {
	

	for (int xx = 0; xx < 8; xx++)              //Now convert each row of the char...
	{
		unsigned char lowBit = lowBitP[xx];            //Get temps
		unsigned char highBit = highBitP[xx];

		uint16_t tempShort = 0;

		for (int g = 0; g < 8; g++)              //2 bits at a time
		{
			unsigned char bits = ((lowBit & 0x80) >> 1) | (highBit & 0x80);		//Build 2 bits from 2 pairs of MSB in bytes (the bitplanes)
			
			lowBit <<= 1;
			highBit <<= 1;
			
			bits >>= 6;				//Shift into LSB
			
			tempShort <<= 2;		//Free up 2 bits at bottom
			
			tempShort |= bits;
		}
					
		patternTable[position++] = tempShort;                              //Put combined bitplane bytes as a short into buffer (we send this to new file)

	}

	
}
	
void updatePalette(int position, char theIndex) {						//Allows game code to update palettes off flash/SD
	
	paletteRGB[position] = nesPaletteRGBtable[theIndex];
	
}

void updatePaletteRGB(int position, char r, char g, char b) {						//Allows game code to update palettes off flash/SD

	uint16_t red = r;
	uint16_t green = g;
	uint16_t blue = b;

	red &= 0xF8;			//Lob off bottom 3 bits
	red <<= 8;				//Pack to MSB
	green &= 0xFC;			//Lob off bottom 2 bits (because evolution)
	green <<= 3;			//Pack to middle
	blue &= 0xF8;			//Lobb off bottom 3 bits
	blue >>= 3;				//Pack to LSB
	
	nesPaletteRGBtable[position] = red | green | blue;
	
}

void setWinYJump(int flag, int jumpFrom, int nextRow) {

	winYJumpList[jumpFrom] = flag | nextRow;	
	
}

void setSpriteWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {	//Sets window in which sprites can be drawn, 0, 119, 0, 119 = full screen

	xLeft = x0 - 1;
	xRight = x1 + 1;
	yTop = y0 - 1;
	yBottom = y1 + 1;	

}	

void setWindow(uint8_t x, uint8_t y) {

	for (int g = 0 ; g < 32 ; g++) {
		
		winX[g] = x >> 3;
		winXfine[g] = x & 0x07;
		winY = y >> 3;
		winYfine = y & 0x07;		
		
	}	

}

void setWindowSlice(int whichRow, uint8_t x) {
	
		winX[whichRow] = x >> 3;
		winXfine[whichRow] = x & 0x07;	
	
}

void setCoarseYRollover(int topRow, int bottomRow) {		//Sets at which row the nametable resets back to the top. If using bottom 2 rows as a status display
	
	winYreset = topRow;
	winYrollover = bottomRow;	
	
}

void sendFrame() {
	
	st7789_setAddressWindow(0, 0, 240, 240);	//Set entire LCD for drawing
	st7789_ramwr();                       		//Switch to RAM writing mode dawg

	int whichBuffer = 0;						//We have 2 row buffers. We draw one, send via DMA, and draw next while DMA is running

	uint16_t temp;

	uint8_t fineYsubCount = 0;
	uint8_t fineYpointer; // = winYfine;
	uint8_t coarseY; // = winY;	
	uint16_t *sp = &spriteBuffer[0];    				  //Use pointer so less math later on

	uint8_t scrollYflag = 0;						      //

	for (int row = 0 ; row < 15 ; row++) {				  //15 rows of 8x8 fat pixels (16x16)

		uint16_t *pointer = &linebuffer[whichBuffer][0];    		//Use pointer so less math later on
		gpio_put(14, 1);											//Scope testing row render time

		if (winYJumpList[row] & 0x80) {								//Flag to jump to a different row in the nametable? (like for a status bar)
			coarseY = winYJumpList[row] & 0x1F;						//Jump to row indicated by bottom 5 bits								
			fineYpointer = 0;										//Reset fine pointer so any following rows are static
		}
		else {														//Not a jump line?
			if (scrollYflag == 0) {									//Is this first line with fine Y scroll enabled? (under a status bar at the top perhaps)
				scrollYflag = 1;									//Set flag so this only happens once
				fineYpointer = winYfine;							//Get Y scroll value
				coarseY = winY;
			}
		}

		for (int yLine = 0 ; yLine < 16; yLine++) {         			//Each char line is 16 pixels in ST7789 memory, so we send each vertical line twice

			uint8_t fineXPointer = winXfine[coarseY];							//Copy the fine scrolling amount so we can use it as a byte pointer when scanning in graphics
			uint16_t *tilePointer = &nameTable[coarseY][winX[coarseY]];			//Get pointer for this character line
			uint8_t winXtemp = winX[coarseY];									//Temp copy for finding edge of tilemap and rolling back over
			temp = patternTable[((*tilePointer & 0x00FF) << 3) + fineYpointer];	//Get first line of pattern		
			temp <<= (fineXPointer << 1);							//Pre-shift for horizontal scroll

			if (yLine & 0x01) {										//Drawing odd line on LCD/second line of fat pixels?							
				sp -= 120;											//Revert sprite buffer point to draw every line twice
				for (int xChar = 0 ; xChar < 120 ; xChar++) {      	//15 characters wide
				
					uint8_t twoBits = temp >> 14;					//Smash this down into 2 lowest bits
					if (*sp == 0xF81F) {							//Transparent sprite pixel? Draw background color...				
						uint16_t tempShort = paletteRGB[(*tilePointer >> 8) + twoBits];	//Add those 2 bits to palette index to get color (0-3)				
						*pointer++ = tempShort;						//...twice (fat pixels)
						*pointer++ = tempShort;		
					}
					else{                							//Else draw sprite pixel...
						*pointer++ = *sp;							//..twice (fat pixels)
						*pointer++ = *sp;	
					}
					*sp = 0xF81F;									//When rendering the doubled sprite line, erase it so it's empty for next frame
					sp++;
					temp <<= 2;										//Shift for next 2 bit color pair

					if (++fineXPointer == 8) {						//Advance fine scroll count and jump to next char if needed
						fineXPointer = 0;
						tilePointer++;								//Advance tile pointer in memory
						if (++winXtemp == 32) {						//Rollover edge of tiles X?
							tilePointer -= 32;						//Roll tile X pointer back 32
						}				
						temp = patternTable[((*tilePointer & 0x00FF) << 3) + fineYpointer];		//Fetch next line from pattern table
					}				
				}				
			}
			else {
				for (int xChar = 0 ; xChar < 120 ; xChar++) {      	//15 characters wide
				
					uint8_t twoBits = temp >> 14;					//Smash this down into 2 lowest bits
					if (*sp == 0xF81F) {							//Transparent sprite pixel? Draw background color...				
						uint16_t tempShort = paletteRGB[(*tilePointer >> 8) + twoBits];	//Add those 2 bits to palette index to get color (0-3)				
						*pointer++ = tempShort;						//...twice (fat pixels)
						*pointer++ = tempShort;		
					}
					else{                							//Else draw sprite pixel...
						*pointer++ = *sp;							//..twice (fat pixels)
						*pointer++ = *sp;	
					}
					sp++;
					temp <<= 2;										//Shift for next 2 bit color pair

					if (++fineXPointer == 8) {						//Advance fine scroll count and jump to next char if needed
						fineXPointer = 0;
						tilePointer++;								//Advance tile pointer in memory
						if (++winXtemp == 32) {						//Rollover edge of tiles X?
							tilePointer -= 32;						//Roll tile X pointer back 32
						}
						temp = patternTable[((*tilePointer & 0x00FF) << 3) + fineYpointer];		//Fetch next line from pattern table
					}				
				}								
			}
	
			if (++fineYsubCount > 1) {
				fineYsubCount = 0;		
				if (++fineYpointer == 8) {						//Did we pass a character edge with the fine Y scroll?
					fineYpointer = 0;							//Reset scroll and advance character
					if (++coarseY > winYrollover) {
						coarseY = winYreset;
					}				
				}					
			}
		}

		gpio_put(14, 0);

		dmaX(0, &linebuffer[whichBuffer][0], 3840);			//Send the 240x16 pixels we just built to the LCD    

		if (++whichBuffer > 1) {							//Switch up buffers, we will draw the next while the prev is being DMA'd to LCD
			whichBuffer = 0;
		}		
	}	
}

void dmaX(int whatChannel, const void* data, int whatSize) {

    while(dma_channel_is_busy(whatChannel) == true) {}

    dma_channel_unclaim(whatChannel);
    dma_channel_config c = dma_channel_get_default_config(whatChannel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, spi_get_dreq(spi0, true));
    
    dma_channel_configure(whatChannel, &c,
                          &spi_get_hw(spi0)->dr, // write address
                          data, // read address
                          whatSize, // element count (each element is of size transfer_data_size)
                          false); // don't start yet

    dma_start_channel_mask((1u << whatChannel));

	// gpio_put(15, 1);
	// dma_channel_set_irq0_enabled(0, true);
	// irq_set_exclusive_handler(DMA_IRQ_0, dma_handler0);
	// irq_set_enabled(DMA_IRQ_0, true);      

}

bool isDMAbusy(int whatChannel) {

	return dma_channel_is_busy(whatChannel);
	
}

void playAudio(const char* path) {
	
	if (audioPlaying == true) {			//Only one sound at a time
		return;
	}
	
	if (!file.open(path, O_RDONLY)) {	//Abort if FNF
		return;
	}

	file.seekSet(40);				//Jump to file size

	audioSamples = file.read() | (file.read() << 8) | (file.read() << 16) | (file.read() << 24);	//Get # of samples
	
	fillAudioBuffer(0);							//Fill both buffers to start
	fillAudioBuffer(1);

	whichAudioBuffer = 0;						//DMA starts with buffer 0
	audioPlaying = true;						//Flag that audio is playing

	endAudioBuffer0flag =  false;
	endAudioBuffer1flag =  false;
	fillBuffer0flag = false;
	fillBuffer1flag = false;

	dmaAudio();									//Start the audio DMA

	

}

void serviceAudio() {										//This is called every frame to see if buffers need re-loading

	if (audioPlaying == false) {
		return;
	}
	
	if (fillBuffer0flag == true) {
		fillBuffer0flag = false;
		endAudioBuffer0flag = fillAudioBuffer(0);			//Fill buffer and return flag if we loaded the last of data
	}
	if (fillBuffer1flag == true) {
		fillBuffer1flag = false;
		endAudioBuffer1flag = fillAudioBuffer(1);
	}	
	
}

bool fillAudioBuffer(int whichOne) {

	int samplesToLoad = audioBufferSize;			//Default # of samples to load into buffer

	//What if samples on 512 edge?

	gpio_put(15, 1);

	if (audioSamples < audioBufferSize) {			//Are there fewer samples than that left in the file?
		samplesToLoad = audioSamples;	
	}

	if (whichOne == 0) {							//Load selected buffer
		for (int x = 0 ; x < samplesToLoad ; x++) {
			audioBuffer0[x] = file.read();
		}
		
		
		//file.read(audioBuffer0, samplesToLoad);
	}
	else {
		for (int x = 0 ; x < samplesToLoad ; x++) {
			audioBuffer1[x] = file.read();
		}		
		
		//file.read(audioBuffer1, samplesToLoad);
	}
	
	gpio_put(15, 0);
	
	if (samplesToLoad < audioBufferSize) {			//Not a full buffer? Means end of file, this is final DMA to do		
		audioSamples = 0;							//Zero this out
		file.close();								//Close file		
		dma_channel_set_trans_count(whichOne, samplesToLoad, false);	//When this channel re-triggers only beat out the exact # of bytes we have
		dma_channel_config object = dma_channel_get_default_config(whichOne + 1);		//Get reference object for channel	
		channel_config_set_chain_to(&object, whichOne + 1);		//Set this channel to chain to itself next time it finishes, which disables chaining
		return true;
	}
	
	audioSamples -= samplesToLoad;				//Else dec # of samples and return no flag

	return false;

}

void dmaAudio() {

    dma_channel_unclaim(1);
	
    dma_channel_config c0 = dma_channel_get_default_config(1);	
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);
    channel_config_set_read_increment(&c0, true);
    channel_config_set_write_increment(&c0, false);	
	
    dma_timer_set_fraction(0, 0x0006, 0xffff);			//The timer will run at the system_clock_freq * numerator / denominator, so this is the speed that data elements will be transferred at via a DMA channel using this timer as a DREQ
    channel_config_set_dreq(&c0, 0x3b);                                 // DREQ paced by timer 0     // 0x3b means timer0 (see SDK manual)
	channel_config_set_chain_to(&c0, 2); 
	
	
    dma_channel_configure(1,
						  &c0,
						  &pwm_hw->slice[audio_pin_slice].cc,			//dst
                          //&pio0_hw->txf[0],   // dst
                          audioBuffer0,   // src
                          audioBufferSize,  // transfer count
                          false           // Do not start immediately
    );

	dma_channel_set_irq0_enabled(1, true);
	irq_set_exclusive_handler(DMA_IRQ_0, dma_handler_buffer0);
	irq_set_enabled(DMA_IRQ_0, true);   

    dma_channel_unclaim(2);
	
    dma_channel_config c1 = dma_channel_get_default_config(2);	
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
    channel_config_set_read_increment(&c1, true);
    channel_config_set_write_increment(&c1, false);	
    channel_config_set_dreq(&c1, 0x3b);                                 // DREQ paced by timer 0     // 0x3b means timer0 (see SDK manual)
	channel_config_set_chain_to(&c1, 1); 
	
    dma_channel_configure(2,
							&c1,
							&pwm_hw->slice[audio_pin_slice].cc,			//dst
							//&pio0_hw->txf[0],   // dst
							audioBuffer1,   // src
							audioBufferSize,  // transfer count
							false           // Do not start immediately
    );

	dma_channel_set_irq1_enabled(2, true);
	irq_set_exclusive_handler(DMA_IRQ_1, dma_handler_buffer1);
	irq_set_enabled(DMA_IRQ_1, true);  


    dma_start_channel_mask(1u << 1);
	
	
}

static void dma_handler_buffer0() {		//This is called when DMA block finishes

	if (endAudioBuffer0flag == true) {
		endAudioBuffer0flag = false;
		audioPlaying =  false;
		pwm_set_gpio_level(10, 128);
		//pio_sm_put_blocking(pio, 0,  0x00);	//Zero out the speaker
		//gpio_put(15, 0);
		//dma_channel_abort(1);
		dma_channel_abort(2);			//Kill the other channel
	}
	else {
		dma_channel_set_read_addr(1, audioBuffer0, false);
		fillBuffer0flag = true;						//Set flag that buffer needs re-filled while other DMA runs		
	}

    // Clear interrupt for trigger DMA channel.
    dma_hw->ints0 = (1u << 1);
    
}

static void dma_handler_buffer1() {

	if (endAudioBuffer1flag == true) {
		endAudioBuffer1flag = false;
		audioPlaying =  false;
		pwm_set_gpio_level(10, 128);
		//pio_sm_put_blocking(pio, 0,  0x00);	//Zero out the speaker
		//gpio_put(15, 0);
		dma_channel_abort(1);			//Kill the other channel
		//dma_channel_abort(2);
	}
	else {
		dma_channel_set_read_addr(2, audioBuffer1, false);
		fillBuffer1flag = true;						//Set flag that buffer needs re-filled while other DMA runs		
	}
    // Clear interrupt for trigger DMA channel.
    dma_hw->ints0 = (1u << 2);
    
}




//Callbacks for the USB Mass Storage Device-----------------------------------------------
int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize) {
// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and 
// return number of copied bytes (must be multiple of block size) 	
	
  // Note: SPIFLash Block API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.readBlocks(lba, (uint8_t*) buffer, bufsize/512) ? bufsize : -1;
}

int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
	
// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and 
// return number of written bytes (must be multiple of block size)	
  digitalWrite(LED_BUILTIN, HIGH);

  // Note: SPIFLash Block API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.writeBlocks(lba, buffer, bufsize/512) ? bufsize : -1;
}

void msc_flush_cb (void) {
	
// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
	
  flash.syncBlocks();  // sync with flash
  fatfs.cacheClear();  // clear file system's cache to force refresh

  digitalWrite(LED_BUILTIN, LOW);
}

/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* This example demo how to expose on-board external Flash as USB Mass Storage.
 * Following library is required
 *   - Adafruit_SPIFlash https://github.com/adafruit/Adafruit_SPIFlash
 *   - SdFat https://github.com/adafruit/SdFat
 *
 * Note: Adafruit fork of SdFat enabled ENABLE_EXTENDED_TRANSFER_CLASS and FAT12_SUPPORT
 * in SdFatConfig.h, which is needed to run SdFat on external flash. You can use original
 * SdFat library and manually change those macros
 *
 * Note2: If your flash is not formatted as FAT12 previously, you could format it using
 * follow sketch https://github.com/adafruit/Adafruit_SPIFlash/tree/master/examples/SdFat_format
 */
