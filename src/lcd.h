#include "stdint.h"

#define LCD_IMAGE_LEN 504
typedef struct LCDState
{
	int spi_fd;
	int pin_dc;
	int pin_rst;
	int pin_led;
} LCDState;

int init_lcd(LCDState *lcd, int spi_channel, int pin_dc, int pin_rst, int pin_led);

int set_contrast(LCDState *lcd, uint8_t contrast);

void draw_image(LCDState *lcd, uint8_t data[LCD_IMAGE_LEN]);
