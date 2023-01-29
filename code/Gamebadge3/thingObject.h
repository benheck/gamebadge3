
#ifndef thingObject_h
#define thingObject_h

#include "Arduino.h"

class thingObject
{
  public:
    thingObject();
	void scan(uint16_t worldX, uint16_t worldY);
	void scanG(uint16_t worldX, uint16_t worldY);
	void setPos(uint16_t atX, uint16_t atY);
	bool hitBox(int16_t x1, int16_t y1, int16_t x2, int16_t y2);

	bool active;								//True = active
	bool visible;								//True = onscreen false = offscreen. If false, don't run hitbox
	int type;									//What kind of objec this is
    int16_t xPos;
	int16_t yPos;
	uint8_t width = 1;
	uint8_t height = 1;
	uint8_t dir = 0;
	uint8_t animate = 3;
	uint8_t subAnimate = 0;

	
  private:	
	
};

#endif