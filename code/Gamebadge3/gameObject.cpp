#include "Arduino.h"
#include "gameObject.h"
#include <gameBadgePico.h>

gameObject::gameObject() {

}

void gameObject::setPos(uint16_t atX, uint16_t atY) {
	
	xPos = atX;
	yPos = atY;
	
}

bool gameObject::hitBox(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {			//Check to see if Bud hit something bigger than himself

	if (visible == false) {
		return false;
	}

	int x1Lap = x1 - xPos;
	int x2Lap = x2 - xPos;	
	
	int pixelWidth = width << 3;
	int pixelHeight = height << 3;

	if (!((x1Lap > -1 && x1Lap < pixelWidth) || (x2Lap > -1 && x2Lap < pixelWidth))) {
		return false;					//No match on X
	}

	int y1Lap = y1 - yPos;
	int y2Lap = y2 - yPos;	

	if (!((y1Lap > -1 && y1Lap < pixelHeight) || (y2Lap > -1 && y2Lap < pixelHeight))) {
		return false;					//No match on Y
	}

	return true;

}	


void gameObject::scan(uint16_t worldX, uint16_t worldY) {

	visible = false;

	if (xPos + (width << 3) < worldX) {				//Would object appear in window (left edge X + width of object)
		return;								//If not, abort
	}
	
	if (xPos > (worldX + 119)) {			//Is object past right edge of window?
		return;								//Also abort
	}

	//OK object must in the window
	
	int offsetX = 0;
	int offsetY = 0;

	if (category == 0 && type < 4 && extraX == true) {		//Evil robuts
		
		if (dir == false) {
			if (++xPos > (xSentryRight - (width * 8))) {		//Sub robot width from right edge, since we are checking left edge of robot xPos
				dir = true;
			}
		}
		else {
			if (--xPos < (xSentryLeft)) {
				dir = false;
			}			
		}
		
	}


	if (category == 0 && type == 4 && extraX == true) {		//Cute kitten
		
		++subAnimate;
		
		if (subAnimate > 3) {
			offsetX = 2;
		}
		
		if (subAnimate > 6) {
			subAnimate = 0;
		}
		
		++animate;
		
		if (animate > 20) {		//20 frames off, 20 frames help, 20 frames !!!
			if (animate < 40) {
				drawSprite(xPos - worldX, yPos - 10, 8, sheetY + 2, width, 1, 0, false, false);
			}
			else {
				drawSprite(xPos - worldX, yPos - 10, 10, sheetY + 2, width, 1, 0, false, false);
			}
		}
		
		if (animate > 60) {
			animate = 0;
		}
			
	}

	drawSprite(xPos - worldX, yPos, sheetX + offsetX, sheetY + offsetY, width, height, palette, dir, false);

	visible = true;


}