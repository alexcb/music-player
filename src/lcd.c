#include "lcd.h"
#include <wiringPi.h>
//#include <wiringPiSPI.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>


int spiSetup( const char *spi_dev, int speed )
{
	int fd;
	int mode = 0; // can be 0, 1, 2 or 3 (according to wiringPi)
	uint8_t spiBPW = 8;


	fd = open( spi_dev, O_RDWR );
	if( fd < 0) {
		return -2; //wiringPiFailure (WPI_ALMOST, "Unable to open SPI device: %s\n", strerror (errno));
	}

	if( ioctl( fd, SPI_IOC_WR_MODE, &mode ) < 0 ) {
		close( fd );
		return -3; //wiringPiFailure (WPI_ALMOST, "SPI Mode Change failure: %s\n", strerror (errno));
	}

	if( ioctl( fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW ) < 0 ) {
		close( fd );
		return -4; //wiringPiFailure (WPI_ALMOST, "SPI BPW Change failure: %s\n", strerror (errno));
	}

	if( ioctl( fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed ) < 0 ) {
		close( fd );
		return -5; //wiringPiFailure (WPI_ALMOST, "SPI Speed Change failure: %s\n", strerror (errno));
	}

	return fd;

}

int init_lcd( LCDState *lcd, const char *spi_dev, int pin_dc, int pin_rst, int pin_led )
{
	lcd->spi_fd = spiSetup( spi_dev, 4000000 );
	lcd->pin_dc = pin_dc;
	lcd->pin_rst = pin_rst;
	lcd->pin_led = pin_led;
	return lcd->spi_fd >= 0 ? 0 : 1;
}

int set_contrast( LCDState *lcd, uint8_t contrast )
{
	if( 0x80 <= contrast ) {
		char contrastBuf[] = { 0x21, 0x14, contrast, 0x20, 0x0c };
		digitalWrite(lcd->pin_dc, 0);
		write(lcd->spi_fd, contrastBuf, 5);
		return 0;
	}
	return 1;
}

void draw_image( LCDState *lcd, uint8_t data[LCD_IMAGE_LEN] )
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

