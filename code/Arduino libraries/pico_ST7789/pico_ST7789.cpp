//ST7789 driver for Pi Pico based Gamebadge3
//Uses this library mostly for setup then dumps data via DMA

#include "Arduino.h"
#include "pico_ST7789.h"

#include <string.h>
#include "hardware/gpio.h"

struct st7789_config st7789_cfg;
uint16_t st7789_width;
uint16_t st7789_height;
bool st7789_data_mode = false;

#define ST77XX_MADCTL_MY 0x80
#define ST77XX_MADCTL_MX 0x40

#define ST77XX_MADCTL_MV 0x20
#define ST77XX_MADCTL_ML 0x10

#define ST77XX_MADCTL_RGB 0x00

uint8_t _xstart;
uint8_t _ystart;

void st7789_init(const struct st7789_config* config, uint16_t width, uint16_t height) {
	
    memcpy(&st7789_cfg, config, sizeof(st7789_cfg));				//Copy referenced structure to our own copy
    st7789_width = width;
    st7789_height = height;

	//2 main differences: If a /CS pin is used, and is V3? (Mode 3 SPI and some other stuff)

	if (st7789_cfg.isV3 == true) {
		initST7789v3();
	}
	else {
		initST7789old();	
	}
	
}

void initST7789old() {		//Old, /CS Mode 0 SPI
	
    spi_init(st7789_cfg.spi, 125 * 1000 * 1000);					//Set SPI to max speed, CPU must be 125Hz
	
    spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(st7789_cfg.gpio_din, GPIO_FUNC_SPI);
    gpio_set_function(st7789_cfg.gpio_clk, GPIO_FUNC_SPI);

	if (st7789_cfg.gpio_cs > -1) {
		gpio_init(st7789_cfg.gpio_cs);		//Leave open if unused
		gpio_set_dir(st7789_cfg.gpio_cs, GPIO_OUT);
		gpio_put(st7789_cfg.gpio_cs, 1);
    }
	
    gpio_init(st7789_cfg.gpio_dc);
    gpio_init(st7789_cfg.gpio_rst);
    gpio_init(st7789_cfg.gpio_bl);

    gpio_set_dir(st7789_cfg.gpio_dc, GPIO_OUT);
    gpio_set_dir(st7789_cfg.gpio_rst, GPIO_OUT);
    gpio_set_dir(st7789_cfg.gpio_bl, GPIO_OUT);

	gpio_put(st7789_cfg.gpio_bl, 1);

    gpio_put(st7789_cfg.gpio_dc, 1);
	gpio_put(st7789_cfg.gpio_rst, 0);
	
	sleep_ms(150);
    gpio_put(st7789_cfg.gpio_rst, 1);
    sleep_ms(100);
    
    // SWRESET (01h): Software Reset
    st7789_cmd(0x01, NULL, 0);
    sleep_ms(150);

    // SLPOUT (11h): Sleep Out
    st7789_cmd(0x11, NULL, 0);
    sleep_ms(10);

	st7789_setRotation(st7789_cfg.rotation);

    // COLMOD (3Ah): Interface Pixel Format
    // - RGB interface color format     = 65K of RGB interface
    // - Control interface color format = 16bit/pixel

    //uint8_t cmd = 0x2c;
    //spi_write_blocking(st7789_cfg.spi, &cmd, sizeof(cmd));
    
    uint8_t cmd = 0x55;
    st7789_cmd(0x3a, &cmd, 1);
    sleep_ms(10);

    st7789_caset(0, st7789_width - 1);
    st7789_raset(0, st7789_height - 1);
	
    // INVON (21h): Display Inversion On
    st7789_cmd(0x21, NULL, 0);
    sleep_ms(10);

    // NORON (13h): Normal Display Mode On
    st7789_cmd(0x13, NULL, 0);
    sleep_ms(10);

    // DISPON (29h): Display On
    st7789_cmd(0x29, NULL, 0);
    sleep_ms(10);	
	
	
}

void initST7789v3() {		

    spi_init(st7789_cfg.spi, 125 * 1000 * 1000);

    spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);

    gpio_set_function(st7789_cfg.gpio_din, GPIO_FUNC_SPI);
    gpio_set_function(st7789_cfg.gpio_clk, GPIO_FUNC_SPI);

    gpio_init(st7789_cfg.gpio_dc);
    gpio_init(st7789_cfg.gpio_rst);
    gpio_init(st7789_cfg.gpio_bl);

    gpio_set_dir(st7789_cfg.gpio_dc, GPIO_OUT);
    gpio_set_dir(st7789_cfg.gpio_rst, GPIO_OUT);
    gpio_set_dir(st7789_cfg.gpio_bl, GPIO_OUT);

    gpio_put(st7789_cfg.gpio_bl, 1);
    gpio_put(st7789_cfg.gpio_dc, 1);

    // Hardware reset
    gpio_put(st7789_cfg.gpio_rst, 0);
    sleep_ms(150);
    gpio_put(st7789_cfg.gpio_rst, 1);
    sleep_ms(100);

    // ---------- ST7789v3 Initialization Sequence ---------- //

    // Sleep Out
    st7789_cmd(0x11, NULL, 0);
    sleep_ms(120);

    // Interface Pixel Format: 16-bit/pixel
    uint8_t pixfmt = 0x55;
    st7789_cmd(0x3A, &pixfmt, 1);

    // Porch Setting
    uint8_t porch[] = {0x1F, 0x1F, 0x00, 0x33, 0x33};
    st7789_cmd(0xB2, porch, sizeof(porch));

    // Gate Control
    uint8_t gate = 0x35;
    st7789_cmd(0xB7, &gate, 1);

    // VCOM Setting
    uint8_t vcom = 0x20;
    st7789_cmd(0xBB, &vcom, 1);

    // Power Control 1
    uint8_t pwr1 = 0x2C;
    st7789_cmd(0xC0, &pwr1, 1);

    // Power Control 2
    uint8_t pwr2 = 0x01;
    st7789_cmd(0xC2, &pwr2, 1);

    // VRH Set
    uint8_t vrh = 0x01;
    st7789_cmd(0xC3, &vrh, 1);

    // VDV Set
    uint8_t vdv = 0x18;
    st7789_cmd(0xC4, &vdv, 1);

    // Display Frame Rate Control
    uint8_t frctrl = 0x13;
    st7789_cmd(0xC6, &frctrl, 1);

    // Power Control 3
    uint8_t pwr3[] = {0xA4, 0xA1};
    st7789_cmd(0xD0, pwr3, sizeof(pwr3));

    // Display Function Control
    uint8_t dispctrl = 0xA1;
    st7789_cmd(0xD6, &dispctrl, 1);

    // Positive Gamma Correction
    uint8_t gamma_pos[] = {0xF0,0x04,0x07,0x04,0x04,0x04,0x25,0x33,0x3C,0x36,0x14,0x12,0x29,0x30};
    st7789_cmd(0xE0, gamma_pos, sizeof(gamma_pos));

    // Negative Gamma Correction
    uint8_t gamma_neg[] = {0xF0,0x02,0x04,0x05,0x05,0x21,0x25,0x32,0x3B,0x38,0x12,0x14,0x27,0x31};
    st7789_cmd(0xE1, gamma_neg, sizeof(gamma_neg));

    // Panel Gate Control
    uint8_t panel_gate[] = {0x1D, 0x00, 0x00};
    st7789_cmd(0xE4, panel_gate, sizeof(panel_gate));

    // Display Inversion On
    st7789_cmd(0x21, NULL, 0);

    // Sleep Out (again for safety)
    st7789_cmd(0x11, NULL, 0);
    sleep_ms(120);

    // Display On
    st7789_cmd(0x29, NULL, 0);
    sleep_ms(20);
	
	st7789_cmd(0x21, NULL, 0); 
	
	// Memory Data Access Control
	st7789_setRotation(st7789_cfg.rotation);

    // ---------- End of Initialization ---------- //


}

void st7789_backlight(bool state) {

	if (state == true) {
		gpio_put(st7789_cfg.gpio_bl, 1);
	}
	else {
		gpio_put(st7789_cfg.gpio_bl, 0);
	}
	
}

void st7789_cmd(uint8_t cmd, const uint8_t* data, size_t len) {
	
    if (st7789_cfg.isV3 == false) {
        spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    } else {
        spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    }
	
    st7789_data_mode = false;

    sleep_us(1);
	
    if (st7789_cfg.gpio_cs > -1) {
        gpio_put(st7789_cfg.gpio_cs, 0);
    }
	
    gpio_put(st7789_cfg.gpio_dc, 0);
    sleep_us(1);
    
    spi_write_blocking(st7789_cfg.spi, &cmd, sizeof(cmd));
    
    if (len) {
        sleep_us(1);
        gpio_put(st7789_cfg.gpio_dc, 1);
        sleep_us(1);
        
        spi_write_blocking(st7789_cfg.spi, data, len);
    }

    sleep_us(1);
	
    if (st7789_cfg.gpio_cs > -1) {
        gpio_put(st7789_cfg.gpio_cs, 1);
    }
	
    gpio_put(st7789_cfg.gpio_dc, 1);
    sleep_us(1);
}

void st7789_caset(uint16_t xs, uint16_t xe)
{
	uint8_t data[4];
	
	data[0] = xs >> 8;
	data[1] = xs & 0xFF;
	data[2] = xe >> 8;
	data[3] = xe & 0xFF;
	
    st7789_cmd(0x2a, data, sizeof(data));			// CASET (2Ah): Column Address Set
}

void st7789_raset(uint16_t ys, uint16_t ye)
{
	uint8_t data[4];
	
	data[0] = ys >> 8;
	data[1] = ys & 0xFF;
	data[2] = ye >> 8;
	data[3] = ye & 0xFF;

    st7789_cmd(0x2b, data, sizeof(data));			// RASET (2Bh): Row Address Set
}

void st7789_ramwr()
{
    sleep_us(1);
	
    if (st7789_cfg.gpio_cs > -1) {
        gpio_put(st7789_cfg.gpio_cs, 0);
    }
	
    gpio_put(st7789_cfg.gpio_dc, 0);
    sleep_us(1);

    // RAMWR (2Ch): Memory Write
    uint8_t cmd = 0x2c;
    spi_write_blocking(st7789_cfg.spi, &cmd, sizeof(cmd));

    sleep_us(1);

	// if (st7789_cfg.gpio_cs > -1) {
		// gpio_put(st7789_cfg.gpio_cs, 0);
	// }
	
	if (st7789_cfg.isV3 == false){
		spi_set_format(st7789_cfg.spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
	} else {
		spi_set_format(st7789_cfg.spi, 16, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
	}	

    gpio_put(st7789_cfg.gpio_dc, 1);
    sleep_us(1);
	
}

void st7789_set_cursor(uint16_t x, uint16_t y) {
    st7789_caset(x, st7789_width - 1);
    st7789_raset(y, st7789_height - 1);
}

void st7789_setRotation(uint8_t which) {
	
  uint8_t cmd = 0;
  
  if (st7789_cfg.isV3 == true) {
	  
    switch (which) {
        case 0: // Portrait
            cmd = 0x00;   // plain portrait yogurt
			_xstart = 0;
			_ystart = 0;
            break;

        case 1: // Portrait 180° (needs RAM offset on Y)
            cmd = 0xC0;
			_xstart = 0;
			_ystart = 80;
            break;

        case 2: // Landscape (Gamebadge3 waveshare rotation 1)
			cmd = 0x70;
			_xstart = 0;
			_ystart = 0;
            break;

        case 3: // Landscape 180° (needs RAM offset on X)
            cmd = 0xA0;
			_xstart = 80;
			_ystart = 0;
            break;
    }	  
  
  }
  else {
	  switch (which & 0x03) {
		  case 0:
			cmd = ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST77XX_MADCTL_RGB;
			_xstart = 0;
			_ystart = 80;
			break;
		  case 1:
			cmd = ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
			_xstart = 80;
			_ystart = 0;
			break;
		  case 2:
			cmd = ST77XX_MADCTL_RGB;
			_xstart = 0;
			_ystart = 0;
			break;
		  case 3:
			cmd = ST77XX_MADCTL_MX | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
			_xstart = 0;
			_ystart = 0;
			break;
	  }	  
	  
  }

  st7789_cmd(0x36, &cmd, 1);
	
}

void st7789_setAddressWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
	
	x += _xstart;		//RAM rotation offsets
	y += _ystart;
	st7789_caset(x, x + (w - 1));		 // Column addr set
	st7789_raset(y, y + (h - 1)); 		// Row addr set

}

