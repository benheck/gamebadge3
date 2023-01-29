#include "Arduino.h"
#include "thingObject.h"
#include <gameBadgePico.h>

thingObject::thingObject() {

}

void thingObject::scan(uint16_t worldX, uint16_t worldY) {

	visible = false;

	int width = 24;

	if ((xPos + 24) < worldX) {				//Would object appear in window (left edge X + width of object)
		return;								//If not, abort
	}
	
	if (xPos > (worldX + 119)) {			//Is object past right edge of window?
		return;								//Also abort
	}

	//OK object must in the window
	drawSprite(xPos - worldX, yPos, 0, 32 + 10, 3, 6, 7, false, false);
	
	visible = true;
	
}

void thingObject::scanG(uint16_t worldX, uint16_t worldY) {

	visible = false;

	int width = 24;

	if ((xPos + 24) < worldX) {				//Would object appear in window (left edge X + width of object)
		return;								//If not, abort
	}
	
	if (xPos > (worldX + 119)) {			//Is object past right edge of window?
		return;								//Also abort
	}

	//OK object must in the window
	drawSprite(xPos - worldX, yPos, 0x06, 16 + animate, 5, false, false);

	visible = true;

	if (++subAnimate == 3) {
		subAnimate = 0;
		if (dir == 0) {
			if (++animate == 7) {
				dir = 1;
			}
		}
		else {
			if (--animate == 3) {
				dir = 0;
			}				
		}			
	}
	
}

bool thingObject::hitBox(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {

	if (visible == false) {
		return false;
	}

	if (xPos > x1 && xPos < x2 && yPos > y1 && yPos < y2) {		
		return true;
	}
	
	return false;

}	


void thingObject::setPos(uint16_t atX, uint16_t atY) {
	
	xPos = atX;
	yPos = atY;
	
}