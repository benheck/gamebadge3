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

	if (state == 99 && moving == true) {						//Object falling?
		yPos += animate;
		if (animate < 8) {
			animate++;
		}
		if (yPos > (119 - (height << 3))) {		//Object hit the floor?
			state = 100;					//Flag for main logic
			yPos = 104;						//On floor
			sheetX = 8 + random(0, 5);		//Turn object to rubble
			sheetY = 32 + 15;
			height = 1;						//Same width, height of 1 tile
			category = 99;					//Set to null type
		}
	}


	//0= tall robot 1=can robot, 2 = flat robot, 3= dome robot, 4= kitten, 5= greenie (greenies on bud sheet 1)
	if (category == 0) {						//Gameplay objects?
		
		if (type < 4 && moving == true) {		//Evil robuts

			if (state == 200) {					//State = 100 = short circuiting
				offsetX = width;				//Use facing forward robot
				
				if (animate & 0x01) {			//Swap faces and shake left and right

					xPos -= 2;

					//Sprite draw in order so this will appear above the base robot
					switch(type) {
						case 0:
							drawSprite(xPos - worldX, yPos - worldY, sheetX + 4, sheetY, width, 3, palette, dir, false);		//Just the blank face
						break;
						
						case 1:
							drawSprite(xPos - worldX, yPos - worldY, sheetX + 0, sheetY + 3, width, 1, palette, dir, false);	//Just the eyes (top row)
						break;
						
						case 2:
							drawSprite(xPos - worldX, yPos - worldY, sheetX + 0, sheetY + 2, width, 1, palette, dir, false);	//Just the eyes (top row)
						break;
						
						case 3:
							drawSprite(xPos - worldX, yPos - worldY, sheetX + 0, sheetY + 3, width, 2, palette, dir, false);	//Just the eyes (top row)						
						break;					
					}

				}
				else {
					xPos += 2;
					
					//Sprite draw in order so this will appear above the base robot
					switch(type) {
						case 0:
							drawSprite(xPos - worldX, yPos - worldY, sheetX + 4, sheetY, width, 3, palette, dir, false);		//Just the blank face
						break;
						
						case 1:
							drawSprite(xPos - worldX, yPos - worldY, sheetX + 3, sheetY + 3, width, 1, palette, dir, false);	//Just the eyes (top row)
						break;
						
						case 2:
							drawSprite(xPos - worldX, yPos - worldY, sheetX + 4, sheetY + 2, width, 1, palette, dir, false);	//Just the eyes (top row)
						break;
						
						case 3:
							drawSprite(xPos - worldX, yPos - worldY, sheetX + 4, sheetY + 3, width, 2, palette, dir, false);	//Just the eyes (top row)						
						break;					
					}
					
				}
				
				if (++animate > 15) {			//Half second shake, then set flag for explosion/removal
					state = 201;					
				}
				
			}
			else {
				if (turning == true) {
					offsetX += width;						//Robot turning face
					if (--animate == 0) {
						turning = false;
					}		
				}
				
				if (dir == false) {
					if (++xPos > (xSentryRight - (width * 8))) {		//Sub robot width from right edge, since we are checking left edge of robot xPos
						dir = true;
						turning = true;
						animate = 4;
					}
				}
				else {
					if (--xPos < (xSentryLeft)) {
						dir = false;
						turning = true;
						animate = 4;					
					}			
				}		
			}

		}

		if (type == 4 && moving == true) {		//Cute kitten

			if (xSentryLeft > 0) {		//Used as a debounce when Bud whacks kitten
				--xSentryLeft;
			}


			if (state == 200) {			
				drawSprite(xPos - worldX, yPos - worldY, 12, 48 + 5, 1, 3, palette, true, false);			//Kitten first
				drawSprite(xPos - worldX - 4, yPos - worldY - 20, 13, 48 + 5, 2, 4, 3, false, false);	//balloon second	
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
						drawSprite(xPos - worldX, yPos - worldY - 10, 8, sheetY + 2, width, 1, 0, false, false);
					}
					else {
						drawSprite(xPos - worldX, yPos - worldY - 10, 10, sheetY + 2, width, 1, 0, false, false);
					}
				}
				
				if (animate > 60) {
					animate = 0;
				}		
			}	
		}		
	
		if (type == 5 && moving == true) {		//Greenie?
			
			drawSprite(xPos - worldX, yPos - worldY, 0x06, 16 + animate, 5, false, false);

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
	
	if (category == 10 && type == 6) {		//Breaking glass shards
	
		bool flipIt = true;
		
		if ((subAnimate & 0x02) == 0x02) {		//Mirror on the 2's
			flipIt = false;
		}

		if ((++subAnimate & 0x01) == 0x01) {	//Flicker on the 1's
			visible = false;
		}
		
		if (subAnimate < 5) {
			yPos -= 2;
		}
		else {
			yPos += 4;
			if (yPos > (119 - (height << 3))) {
				yPos = 104;						//On floor
				sheetX = 8 + random(0, 5);		//Turn object to rubble
				sheetY = 32 + 15;
				height = 1;						//Same width, height of 1 tile
				category = 99;
			}
		}

	}

	if (category == 100) {			//Object has become an EXPLOSION!

		switch(animate) {		//Animate on 2's
		
			case 0:
				drawSprite(xPos - worldX, yPos - worldY, 6, 48 + 8, 2, 2, 4, dir, false);
			break;
			
			case 1:
				drawSprite(xPos - worldX, yPos - worldY, 6, 48 + 8, 2, 2, 4, dir, false);
			break;
			
			case 2:
				drawSprite(xPos - worldX - 8, yPos - worldY - 8, 8, 48 + 8, 4, 4, 4, dir, false);
			break;
							
			case 3:
				drawSprite(xPos - worldX - 8, yPos - worldY - 8, 8, 48 + 12, 4, 4, 4, dir, false);
			break;
			
			case 4:
				drawSprite(xPos - worldX - 8, yPos - worldY - 8, 12, 48 + 12, 4, 4, 4, dir, false);
			break;			
			
		}

		if (++subAnimate == 2) {
			subAnimate = 0;

			if (++animate == 5) {		
				active = 0;
			}
			
		}
			
	}

	if (visible == true && category < 100) {
		drawSprite(xPos - worldX, yPos - worldY, sheetX + offsetX, sheetY + offsetY, width, height, palette, dir, false);		
	}

}