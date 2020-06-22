#ifdef NONFUNZIONA

#include <avr/io.h>
#include <util/delay.h>
#include "i2clib.h"
#include "serial.h"
#include "printf.h"
#include <inttypes.h>
#include <util/twi.h>
#include <stdio.h>
#include <inttypes.h>
void playFreq(uint16_t freq)
{
	uint16_t period = 20000000UL/128UL/freq;
	OCR1AH = period>>8;
	OCR1AL = period & 0xff;
	
}

void beep(uint16_t freq, uint16_t period)
{
	playFreq(freq);
	while(--period > 0)
		_delay_ms(1);
	
	
}


uint16_t recordCapacity()
{
	uint16_t contatore;
	
	DDRC &= ~_BV(4);
	PORTC &= ~_BV(4);
	for( contatore = 0; PINC & _BV(4); contatore++)
		if(contatore > 0xf000UL)
			contatore = 0xf000UL;
	PORTC |= _BV(4);
	DDRC |= _BV(4);
			
	return contatore;
}

uint16_t calcolaValore()
{
	uint16_t valore;
	uint8_t i;
	uint32_t contatore = 0;
	for(i = 0; i < 64;i++)
		contatore += recordCapacity();
	valore = contatore >> 6;
	return valore;
}

int main()
{
		startSerial(9600);
		startIO();
		
		
		
		DDRD |= _BV(5);
		DDRC |= _BV(5);
		
		
		TCCR1A = _BV(COM1A0) | _BV(WGM11) |_BV(WGM10);
		TCCR1B = _BV(WGM12) | _BV(WGM13) |_BV(CS11) |_BV(CS10);
		
		uint16_t valore;
		uint16_t f;
		while(1)
		{
			
			valore = calcolaValore();
			printf("valore %d\n",valore);
			
			f = (valore - 2800) *2;
			playFreq(f);
			//beep(valore-5000,100);
			
			//PIND = _BV(5);
		}
	
}
#endif