
#ifndef gameObject_h
#define gameObject_h

#include "Arduino.h"

class gameObject
{
  public:
    gameObject();

	void setPos(uint16_t atX, uint16_t atY);					//Sets position in world
	void defineTiles(int x1, int y1, int x2, int y2);			//Defines most objects. Calling this calcs height and width for hitboxes
	void defineTileSingle(int whichTile);						//Defines a single tile object like a Greenie
	void setSize(int theWidth, int theHeight);

	void scan(int16_t worldX, int16_t worldY);
	bool hitBox(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
	bool hitBoxSmall(int16_t x1, int16_t y1, int16_t x2, int16_t y2);

	//State:
	//0 = no effect
	//99 = falling
	//100 = fell, smashed on floor (floor is constant)
	//101 = fell, damaged a robot
	//200 = kitten rescue float
	//255 - test animate

	bool active = false;						//True = active (logic can execute, ie enemy moves)
	bool visible = false;						//True = onscreen false = offscreen. If false, don't run hitbox
	uint8_t state = 0;							//Falling, turning, intact, destroyed, test animate etc
	uint8_t whenBudTouch = 0;					//0 = nothing, 1 = knock over, 2 = collect (greenie), 3 = kill (monster)
	
    uint16_t xPos, yPos;						//Position in world, is upper left corner of sprite
	bool singleTile = false;					//Greenies are single cell, rest are bigger	
	uint8_t sheetX, sheetY;						//Upper left corner of sprite on sheet
	uint8_t width = 1;							//Size in tiles
	uint8_t height = 1;							//Used for hitboxes and screen calcs
	uint8_t palette;							//0-7
	uint8_t category;							////0= gameplay (enemies and greenies), 1 = bad art 4x2, 2 = small 2x2, 3= tall 2x3, 4= appliance 3x2, 5= eraser
	uint8_t type;								//What kind of object this is within category
	
	bool dir = false;							//False = facing right (not flipped) Sprites should be drawn facing right as default (false)
	
	bool turning = false;
	uint8_t animate = 0;
	uint8_t subAnimate = 0;
	
	uint16_t xSentryLeft = 0;
	uint16_t xSentryRight = 120;
	
	bool moving = false;					
	bool extraY = false;
			
	uint8_t stunTimer = 0;
	uint8_t speedPixels = 1;
	uint8_t extraC = 0;
	uint8_t extraD = 0;	
	
	
  private:	
	
};

#endif