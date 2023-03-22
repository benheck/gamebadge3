#include "Arduino.h"
#include "thingObject.h"
#include <gameBadgePico.h>

thingObject::thingObject() {

}

void thingObject::scan(uint16_t worldX, uint16_t worldY) {

	visible = false;

	int width = 16;

	if (state == 9) {						//Do animation state even if off-screen
		
		if (++subAnimate == 8) {
			subAnimate = 0;
			animate++;
		}	
		if (animate == 16) {
			state = 1;
			playAudio("audio/startup.wav");
		}
		
	}

	//OK now check if visible

	if ((xPos + 16) < worldX) {				//Would object appear in window (left edge X + width of object)
		return;								//If not, abort
	}
	
	if (xPos > (worldX + 119)) {			//Is object past right edge of window?
		return;								//Also abort
	}


	switch(state) {
		
		case 1:
			if (turning == false) {
				drawSprite(xPos - worldX, yPos, 0, 48, 2, 6, 7, dir, false);
			}
			else {
				drawSprite(xPos - worldX, yPos, 2, 48, 2, 6, 7, false, false);
			}		
		break;
			
		case 9:
			if (animate < 8) {
				drawSprite((xPos - worldX) + 4, yPos + 5, 6, 48, 1, 2, 7, false, false);						//X face
			}
			else {
				if (animate < 16) {
					drawSprite((xPos - worldX) + 4, yPos + 5, animate - 8, 48 + 6, 1, 2, 7, false, false);		//Reboot progress bar
				}
			}
				
			drawSprite(xPos - worldX, yPos, 4, 48, 2, 3, 7, false, false);			//Draw blank face
			drawSprite(xPos - worldX, yPos + 24, 2, 48 + 3, 2, 3, 7, false, false);	//Draw body
	
		break;
		
	}

	visible = true;
	
}

void thingObject::scanG(uint16_t worldX, uint16_t worldY) {

	visible = false;

	if ((xPos + width) < worldX) {				//Would object appear in window (left edge X + width of object)
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
		if (dir == true) {
			if (++animate == 7) {
				dir = false;
			}
		}
		else {
			if (--animate == 3) {
				dir = true;
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
	drawSprite(xPos - worldX, yPos, 4, 32 + 8, 4, 3, 7, false, false);

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
	drawSprite(xPos - worldX, yPos, 4, 32 + 8 + 3, 4, 1, 7, false, false);

	visible = true;

}

void thingObject::scanC3(uint16_t worldX, uint16_t worldY) {

	visible = false;

	if ((xPos + width) < worldX) {				//Would object appear in window (left edge X + width of object)
		return;								//If not, abort
	}
	
	if (xPos > (worldX + 119)) {			//Is object past right edge of window?
		return;								//Also abort
	}
	
	switch(state) {
		
		case 0:
			drawSprite(xPos - worldX, yPos, 4, 32 + 12, 4, 3, 6, false, false);			//Intact window
		break;
			
		case 1:
			drawSprite(xPos - worldX, yPos, 0, 32 + 12, 4, 3, 6, false, false);			//Broken window
		break;
		
	}	
	
}	

void thingObject::scanC4(uint16_t worldX, uint16_t worldY) {

	visible = false;

	if ((xPos + width) < worldX) {				//Would object appear in window (left edge X + width of object)
		return;								//If not, abort
	}
	
	if (xPos > (worldX + 119)) {			//Is object past right edge of window?
		return;								//Also abort
	}

	yPos += 3;

	if (yPos > 119) {
		active = false;
	}
	
	bool flipIt = true;
	
	if ((animate & 0x02) == 0x02) {
		flipIt = false;
	}

	if ((++animate & 0x01) == 0x01) {
		drawSprite(xPos - worldX, yPos, 0, 32 + 8, 4, 3, 6, flipIt, false);	
	}

	
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
		return false;					//No match on Y
	}

	return true;

}	

bool thingObject::hitBoxSmall(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {		//Check to see if Bud hit something smaller than himself

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


void thingObject::setPos(uint16_t atX, uint16_t atY) {
	
	xPos = atX;
	yPos = atY;
	
}

void thingObject::setSize(int theWidth, int theHeight) {		//Sets size in pixels

	width = theWidth;
	height = theHeight;
	
}