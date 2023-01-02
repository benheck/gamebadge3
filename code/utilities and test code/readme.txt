Use the SdFat_format sketch to format a Pi Pico's flash for use with a filesystem

1) Load SdFat_format sketch
2) Set Arduino environment:
	Board: Raspberry Pi Pico W
	Flash size: 2MB (1MB sketch, 1MB file system)
	CPU Speed: 125MHz
	USB Stack: Adafruit TinyUSB
	Port: Pico's com port or UF2 (UF2 is default if you boot holding the BOOT button)
	
3) Upload sketch
4) Once running, open Arudion serial port
5) A prompt to type OK will appear. Type OK(return) and file system will be formatted

The top half of the Pico's flash is now formatted as a file system. This can be used in your
sketch (like an SD card) or as a USB MSD when plugged into a computer.

To test USB "thumb drive mode":

1) Load msc_external_flash sketch
2) Set Arduino environment: (same as above)
	Board: Raspberry Pi Pico W
	Flash size: 2MB (1MB sketch, 1MB file system)
	CPU Speed: 125MHz
	USB Stack: Adafruit TinyUSB
	Port: Pico's com port or UF2 (UF2 is default if you boot holding the BOOT button)
	(The Pico should appear as a port now that you've flashed an Arduino sketch to it)
3) Upload sketch
4) The Pico should mount as a 1MB thumb drive on your computer.

Note that when opening a new sketch the Arduino environment settings may change, double check
them when going through this checklist.

Once the flash filesystem is formatted and working you can run the gamebadge3 sketch.