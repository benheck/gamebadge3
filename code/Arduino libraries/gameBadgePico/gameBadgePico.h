//Game & graphics driver for gameBadgePico (MGC 2023)

#ifndef gameBadgePico_h
#define gameBadgePico_h

#include "Arduino.h"
#include "hardware/spi.h"

	void gamebadge3init();
	void setGPIObutton(int which);
	void setButtonDebounce(int which, bool useDebounce, uint8_t frames);
	
	uint32_t pwm_set_freq_duty(int gpioNum, uint32_t f, int d);
	
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
	void sendFrame();
	void dmaX(int whatChannel, const void* data, int whatSize);
	bool isDMAbusy(int whatChannel);

	void drawTile(int xPos, int yPos, uint16_t whatTile, char whatPalette);
	void drawTile(int xPos, int yPos, uint16_t tileX, uint16_t tileY, char whatPalette);
	void fillTiles(int startX, int startY, int endX, int endY, uint16_t whatTile, char whatPalette);
	void setASCIIbase(uint16_t whatBase);
	void setTextMargins(int left, int right);
	void drawText(const char *text, uint8_t x, uint8_t y, bool doWrap);
	
	void drawSprite(int xPos, int yPos, uint16_t tileX, uint16_t tileY, uint8_t whichPalette, bool hFlip, bool vFlip);
	void drawSprite(int xPos, int yPos, uint16_t tileX, uint16_t tileY, uint16_t xWide, uint16_t yHigh, uint8_t whichPalette, bool hFlip, bool vFlip);
	void clearSprite();
	void updatePalette(int position, char theIndex);
	void updatePaletteRGB(int position, char r, char g, char b);
	void convertBitplanePattern(uint16_t position, unsigned char *lowBitP, unsigned char *highBitP);

	void playAudio(const char* path);
	void serviceAudio();
	bool fillAudioBuffer(int whichOne);
	void dmaAudio();
	static void dma_handler_buffer0();
	static void dma_handler_buffer1();	
	
	
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
	
	//Old
	// #define up_but 			6
	// #define down_but			8
	// #define left_but			7
	// #define right_but		9
	
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
