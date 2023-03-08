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

bool gameObject::hitBoxSmall(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {

	if (visible == false) {
		return false;
	}

	if ((xPos < x1 && (xPos + width) < x1) || xPos > x2) {			//No match on X
		return false;
	}

	if (yPos < y1 && (yPos + height) < y1 || yPos > y2) {			//No match on Y
		return false;					
	}

	return true;
	
}	


void gameObject::scan(uint16_t worldX, uint16_t worldY) {

	visible = true;

	if (xPos + (width << 3) < worldX) {		//Would object appear in window (left edge X + width of object)
		visible = false;
	}
	
	if (xPos > (worldX + 119)) {			//Is object past right edge of window?
		visible = false;
	}

	int offsetX = 0;
	int offsetY = 0;

	if (state == 99) {						//Object falling?
		yPos += animate;
		if (animate < 8) {
			animate++;
		}
		if (yPos > (119 - (height << 3))) {		//Object hit the floor?
			state = 100;
		}
	}

	if (category == 0) {						//Gameplay objects?
		
		if (type < 4 && extraX == true) {		//Evil robuts
			
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

		if (type == 4 && extraX == true) {		//Cute kitten

			if (state == 200) {			
				drawSprite(xPos - worldX, yPos, 12, 48 + 5, 1, 3, palette, true, false);			//Kitten first
				drawSprite(xPos - worldX - 4, yPos - 20, 13, 48 + 5, 2, 4, 3, false, false);	//balloon second	
				yPos -= animate;			
				if (yPos < -30) {
					active = false;
				}
				if (animate < 4) {
					++animate;
				}		
				return;			//Don't use normal sprite draw
			}
			else {
				
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
		}		
	
		if (type == 5 && extraX == true) {		//Greenie?
			
			drawSprite(xPos - worldX, yPos, 0x06, 16 + animate, 5, false, false);

			visible = true;

			if (++subAnimate > 2) {
				subAnimate = 0;
				if (dir == true) {
					if (++animate > 6) {
						animate = 7;
						dir = false;
					}
				}
				else {
					if (--animate < 4) {
						animate = 3;
						dir = true;
					}				
				}			
			}	

			return;
			
		}

	
	}

	if (visible == true) {
		drawSprite(xPos - worldX, yPos, sheetX + offsetX, sheetY + offsetY, width, height, palette, dir, false);		
	}

}