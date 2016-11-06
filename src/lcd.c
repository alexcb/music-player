#include "lcd.h"
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <unistd.h>

int init_lcd(LCDState *lcd, int spi_channel, int pin_dc, int pin_rst, int pin_led)
{
	lcd->spi_fd = wiringPiSPISetup(spi_channel, 4000000);
	lcd->pin_dc = pin_dc;
	lcd->pin_rst = pin_rst;
	lcd->pin_led = pin_led;
	return lcd->spi_fd >= 0 ? 0 : 1;
}

int set_contrast(LCDState *lcd, uint8_t contrast)
{
	if( 0x80 <= contrast ) {
		char contrastBuf[] = { 0x21, 0x14, contrast, 0x20, 0x0c };
		digitalWrite(lcd->pin_dc, 0);
		write(lcd->spi_fd, contrastBuf, 5);
		return 0;
	}
	return 1;
}

void draw_image(LCDState *lcd, uint8_t data[LCD_IMAGE_LEN])
{
	// GOTO
	digitalWrite(lcd->pin_dc, 0);
	char buf[] = { 0+128, 0+64 }; //TODO what do the 128 and 64 values represent?
	write(lcd->spi_fd, buf, 2);

	// enable vertical addressing mode
	digitalWrite(lcd->pin_dc, 0);
	char buf2[] = { 0x22 };
	write(lcd->spi_fd, buf2, 1);
	
	digitalWrite(lcd->pin_dc, 1);
	write(lcd->spi_fd, data, LCD_IMAGE_LEN);
}

