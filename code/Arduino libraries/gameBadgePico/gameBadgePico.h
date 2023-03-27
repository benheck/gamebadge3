//Game & graphics driver for gameBadgePico (MGC 2023)

#ifndef gameBadgePico_h
#define gameBadgePico_h

#include "Arduino.h"
#include "hardware/spi.h"

	void gamebadge3init();
	void setGPIObutton(int which);
	void setButtonDebounce(int which, bool useDebounce, uint8_t frames);
	
	void setupPWMchannels(int whichGPIO, int whichChannel);
	void pwm_set_freq_music(int whichChannel, uint8_t duty_cycle);
	
	
	uint32_t pwm_set_freq_duty(int gpioNum, uint32_t f, int d);
	
	void backlight(bool state);
	
	bool button(int which);
	void serviceDebounce();
	bool loadRGB(const char* path);
	void loadPalette(const char* path);
	void loadPattern(const char* path, uint16_t start, uint16_t length);
	
	void setWinYjump(int jumpFrom, int nextRow);
	void clearWinYjump(int whichRow);
	
	void setSpriteWindow(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1);
	void setWindow(uint8_t x, uint8_t y);
	void setWindowSlice(int whichRow, uint8_t x);
	void setCoarseYRollover(int topRow, int bottomRow);
	
	//NEW 3-27-23
	void pauseLCD(bool state);
	bool getRenderStatus();
	void LCDlogic();
	void lcdRenderRow();
	void LCDsetDrawFlag();
	void dmaLCD(int whatChannel, const void* data, int whatSize);
	
	//OLD AND BUSTED
	void sendFrame();
	void dmaX(int whatChannel, const void* data, int whatSize);
	bool isDMAbusy(int whatChannel);


	void drawTile(int xPos, int yPos, uint16_t whatTile, char whatPalette, int flags = 0x00);
	void drawTile(int xPos, int yPos, uint16_t tileX, uint16_t tileY, char whatPalette, int flags = 0x00);
	void setTileType(int tileX, int tileY, int flags);
	void tileDirect(int tileX, int tileY, uint16_t theData);
	void setTilePalette(int tileX, int tileY, int whatPalette);
	int getTileType(int tileX, int tileY);
	int getTileValue(int tileX, int tileY);
	void fillTiles(int startX, int startY, int endX, int endY, uint16_t whatTile, char whatPalette);
	void fillTiles(int startX, int startY, int endX, int endY, uint16_t patternX, uint16_t patternY, char whatPalette);
	void setASCIIbase(uint16_t whatBase);
	void setTextMargins(int left, int right);
	void drawText(const char *text, uint8_t x, uint8_t y, bool doWrap = false);
	void drawSpriteText(const char *text, uint8_t x, uint8_t y, int whatPalette);
	void drawDecimal(int32_t theValue, uint8_t x, uint8_t y);
	void drawSpriteDecimal(int32_t theValue, uint8_t x, uint8_t y, int whatPalette);
	
	
	void drawSprite(int xPos, int yPos, uint16_t whichTile, uint8_t whichPalette, bool hFlip = false, bool vFlip = false);
	void drawSprite(int xPos, int yPos, uint16_t tileX, uint16_t tileY, uint16_t xWide, uint16_t yHigh, uint8_t whichPalette, bool hFlip = false, bool vFlip = false);		
	void drawSprite(int xPos, int yPos, uint16_t tileX, uint16_t tileY, uint8_t whichPalette, bool hFlip = false, bool vFlip = false);

	void clearSprite();
	void updatePalette(int position, char theIndex);
	void updatePaletteRGB(int position, char r, char g, char b);
	void convertBitplanePattern(uint16_t position, unsigned char *lowBitP, unsigned char *highBitP);

	void playAudio(const char* path, int newPriority);
	void stopAudio();
	void serviceAudio();
	bool fillAudioBuffer(int whichOne);
	void dmaAudio();
	static void dma_handler_buffer0();
	static void dma_handler_buffer1();	
	
	bool checkFile(const char* path);
	void saveFile(const char* path);
	bool loadFile(const char* path);
	void writeByte(uint8_t theByte);
	void writeBool(bool state);
	uint8_t readByte();
	bool readBool();
	void closeFile();
	void eraseFile(const char* path);
	
	
	int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize);
	int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize);
	void msc_flush_cb (void);
	
	//GPIO connections:
	// #define C_but			2
	// #define B_but			3
	// #define A_but			4	
	// #define start_but		5
	// #define select_but		21
	// #define up_but 			7
	// #define down_but			11
	// #define left_but			9
	// #define right_but		13
	
	//Buttons to index #: (these are the defines your game will use)
	#define up_but 			0
	#define down_but		1
	#define left_but		2
	#define right_but		3
	#define select_but		4
	#define start_but		5	
	#define A_but			6
	#define B_but			7
	#define C_but			8	

#endif
