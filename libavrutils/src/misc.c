#include "misc.h"
#include <avr/wdt.h>
#include <avr/interrupt.h>

typedef void (*BootloaderPtrt)(void)  ;

uint16_t freeRam () {
  extern int __heap_start, *__brkval; 
  int v;
  uint16_t v2 = (uint16_t) &v;
  return v2 - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void watchdog_reboot()
{
	cli();
	wdt_enable(WDTO_15MS);
	while(1);
}


static void disable_single_interrupts()
{
	//uart
	UCSR0B = 0;
	UCSR0B = UCSR0B;
	
	//i2c
	TWCR = 0;
	TWCR = TWCR;
	
	//timer 0
	TCCR0B = 0;
	TIMSK0 = 0;
	TIFR0 = TIFR0;
	
	//timer 1
	TCCR1B = 0;
	TIMSK1 = 0;
	TIFR1 = TIFR1;
	
	//timer 2
	TCCR2B = 0;
	TIMSK2 = 0;
	TIFR2 = TIFR2;
	
	//ADC
	ADCSRA = 0;
	ADCSRA = ADCSRA;
	
}

void reboot_to_bootloader()
{
	cli();

	
	disable_single_interrupts();
	
	BootloaderPtrt bootptr= (BootloaderPtrt) 0xf000u;
	//void*address = (void*) 0xf000u;
	
	// asm volatile("jmp     0xf000");
	
//	goto *address;
	wdt_enable(WDTO_250MS);
	bootptr();
	
	while(1)
	{
		;
	}

}