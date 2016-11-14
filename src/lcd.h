#include "stdint.h"

#define SPIDEV0 "/dev/spidev0.0"
#define SPIDEV1 "/dev/spidev0.1"

#define LCD_IMAGE_LEN 504

typedef struct LCDState
{
	int spi_fd;
	int pin_dc;
	int pin_rst;
	int pin_led;
} LCDState;

int init_lcd( LCDState *lcd, const char *spi_dev, int pin_dc, int pin_rst, int pin_led );

int set_contrast( LCDState *lcd, uint8_t contrast );

void draw_image( LCDState *lcd, uint8_t data[LCD_IMAGE_LEN] );
