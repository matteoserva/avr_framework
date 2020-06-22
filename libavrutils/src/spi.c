#include <avr/io.h>
#include "spi.h"
#define DDR_SPI DDRB
#define PORT_SPI PORTB
#define DD_MOSI 5
#define DD_SCK 7
#define DD_MISO 6
#define DD_SS 4

static uint8_t ssPIN;

static void SPI_start_master()
{
	/* Set MOSI and SCK output, all others input */
	PORT_SPI |= _BV(DD_SS);

	DDR_SPI |= (1<<DD_MOSI)|(1<<DD_SCK) ;

	/* Enable SPI, Master, set clock rate fck/16 */
	SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0)|(1<<SPR1);
}

static void SPI_stop_master()
{
	SPCR = 0;
	DDR_SPI &= ~(_BV(DD_MOSI) | _BV(DD_SCK));
}


uint8_t SPI_send(uint8_t cData)
{

	/* Start transmission */
	SPDR = cData;
	/* Wait for transmission complete */
	while(!(SPSR & (1<<SPIF)))
		;
	return SPDR;
}

void SPI_slave_select(uint8_t pin)
{
	if(ssPIN)
		SPI_slave_deselect();
	if(! (SPCR & _BV(MSTR)))
		SPI_start_master();

	ssPIN |= _BV(pin);
	
	PORT_SPI &= ~_BV(pin);
	DDR_SPI |= _BV(pin);

}

void SPI_slave_deselect()
{
	PORT_SPI &= ~_BV(DD_SCK);
	
	DDR_SPI &= (~ssPIN);
	PORT_SPI |= ssPIN;
	ssPIN=0;
	SPI_stop_master();

}

