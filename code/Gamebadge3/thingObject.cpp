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

void thingObject::scanC1(uint16_t worldX, uint16_t worldY) {

	visible = false;

	int width = 32;

	if ((xPos + width) < worldX) {				//Would object appear in window (left edge X + width of object)
		return;								//If not, abort
	}
	
	if (xPos > (worldX + 119)) {			//Is object past right edge of window?
		return;								//Also abort
	}

	//OK object must in the window
	drawSprite(xPos - worldX, yPos, 12, 32, 4, 3, 7, false, false);

	visible = true;

}

void thingObject::scanC2(uint16_t worldX, uint16_t worldY) {

	visible = false;

	int width = 32;

	if ((xPos + width) < worldX) {				//Would object appear in window (left edge X + width of object)
		return;								//If not, abort
	}
	
	if (xPos > (worldX + 119)) {			//Is object past right edge of window?
		return;								//Also abort
	}

	//OK object must in the window
	drawSprite(xPos - worldX, yPos, 12, 32 + 3, 4, 1, 7, false, false);

	visible = true;

}


bool thingObject::hitBox(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {			//Check to see if Bud hit something bigger than himself

	if (visible == false) {
		return false;
	}

	bool xMatch;
	bool yMatch;

	int x1Lap = x1 - xPos;
	int x2Lap = x2 - xPos;	

	if ((x1Lap > -1 && x1Lap < width) || (x2Lap > -1 && x2Lap < width)) {
		xMatch = true;
	}
	else {
		return false;					//No match on X
	}

	int y1Lap = y1 - yPos;
	int y2Lap = y2 - yPos;	

	if ((y1Lap > -1 && y1Lap < height) || (y2Lap > -1 && y2Lap < height)) {
		yMatch = true;
	}
	else {
		return false;					//No match on X
	}

	return true;

}	

bool thingObject::hitBoxSmall(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {		//Check to see if Bud hit something smaller than himself

	if (visible == false) {
		return false;
	}

	bool xMatch;
	bool yMatch;

	int x1Lap = xPos - x1;
	int x2Lap = xPos - x2;	

	if ((x1Lap > -1 && x1Lap < (x2 - x1)) || (x2Lap > -1 && x2Lap < (x2 - x1))) {
		xMatch = true;
	}
	else {
		return false;					//No match on X
	}

	int y1Lap = yPos - y1;
	int y2Lap = yPos - y2;

	if ((y1Lap > -1 && y1Lap < (y2 - y1)) || (y2Lap > -1 && y2Lap < (y2 - y1))) {
		yMatch = true;
	}
	else {
		return false;					//No match on X
	}

	return true;

	// if (xPos > x1 && xPos < x2 && yPos > y1 && yPos < y2) {		
		// return true;
	// }
	
	return false;

}	


void thingObject::setPos(uint16_t atX, uint16_t atY) {
	
	xPos = atX;
	yPos = atY;
	
}

void thingObject::setSize(int theWidth, int theHeight) {		//Sets size in pixels

	width = theWidth;
	height = theHeight;
	
}