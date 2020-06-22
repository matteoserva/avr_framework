
#include "milliseconds.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

static volatile uint16_t milliseconds_count = 0;

static uint8_t increment_base;
static uint8_t increment_8th;

//8mhz 31 33, 20 mhz 78 79


void milliseconds_init()
{
	uint16_t clocks_per_milli = F_CPU / 1000UL;
	uint16_t time_cycles_per_8milli = clocks_per_milli/32U;
	
	
	increment_base = time_cycles_per_8milli / 8U;
	increment_8th = (time_cycles_per_8milli % 8U) + increment_base;
	
	TCCR0A = 0;
	TCCR0B = _BV(CS02); //clock/256
	TIMSK0 |= _BV(OCIE0A); //attivo l'interrupt A

	OCR0A = increment_base; //20000/256 = 78,125;
}

uint16_t milliseconds()
{
	uint16_t m;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		m = milliseconds_count;
	}

	return m;
}

ISR(TIMER0_COMPA_vect)
{
	if(milliseconds_count % 8)
		OCR0A+= increment_base;
	else
		OCR0A+= increment_8th;
	milliseconds_count++;
}
