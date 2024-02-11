
This a a fork of BenHeck's GameBadge3 repo (link above)

I have a branch here "ST7789_280x240" where I made some changes
to support wider screens, but still with a ST7789V chip.

To use it, modify code/Arduino libraries/pico_ST7789/pico_ST7789.cpp to set the size of the border
needed to center the screen, and set the appropriate rotation :

exemple for a 320x240 reversed screen, with 40-pixels wide black borders :
<code>
  #define LCD_OFFSET_X 40
  #define LCD_ROTATION 3
</code>

and then recompile, of course.

Centering vertically has not been done yet.

Don't forget to do that on the correct branch :
<code>
git clone https://github.com/theflynn49/gamebadge3.git
git checkout ST7789_280x240
</code>

Some GB3 games connot be recompiled because we don't have access to the sources, so it will not work for those.

Warning : Games for GB3 must be compiled with 

  - flash size=1MB / Sketch Size=1MB
  - CPU Speed = 125Mhz (important !)
  - USB Stack : Adafruit TinyUSB

Have fun.

jF

