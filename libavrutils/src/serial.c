#include <avr/io.h>
#include "serial.h"
#include "circular_buffer.h"
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>

#define SERBUFFER_SIZE 64
#define BUFFER_IN_SIZE 64
static char serBuffer [sizeof(struct CircularBufferHeader_t) + SERBUFFER_SIZE];

static char buffer_in [sizeof(struct CircularBufferHeader_t) + BUFFER_IN_SIZE];
volatile uint8_t transmission_running = 0;

void startSerial(unsigned long baud)
{
	circular_buffer_init(serBuffer,SERBUFFER_SIZE);
	circular_buffer_init(buffer_in,BUFFER_IN_SIZE);
	
    UBRR0L = (uint8_t)((F_CPU+baud*8UL)/(baud*16L)-1);
    UBRR0H = ((F_CPU+baud*8UL)/(baud*16L)-1) >> 8;
    UCSR0B = (1<<RXEN0) | (1<<TXEN0) | _BV(TXCIE0)| _BV(RXCIE0);
    UCSR0C = (1<<UCSZ00) | (1<<UCSZ01);

    DDRD &= ~_BV(PIND0);
    PORTD |= _BV(PIND0);
	
}

static void send_byte()
{
	int16_t val = circular_buffer_pop(serBuffer);
	if(val >= 0)
		 UDR0 = val;
	else
		transmission_running = 0;
}

ISR(USART0_TX_vect)
{
	send_byte();
}

ISR(USART0_RX_vect)
{
	circular_buffer_push(buffer_in,UDR0);
}


void putch(unsigned char ch)
{
	while(circular_buffer_push(serBuffer,ch) < 0)
	{
		if(UCSR0A & _BV(TXC0))
		{
			UCSR0A = UCSR0A;
			send_byte();
		}
			
		
	}
	//circular_buffer_push(serBuffer,ch);
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if((!transmission_running) && circular_buffer_usage(serBuffer))
		{
		UDR0 = circular_buffer_pop(serBuffer);
		transmission_running = 1;
		}
	}
}

uint8_t serial_putch_available()
{
	return (circular_buffer_usage(serBuffer) < SERBUFFER_SIZE);
}



uint8_t getch(void)
{
    while(!serial_getch_available())
		;
    return circular_buffer_pop(buffer_in);
}

void serial_putch(unsigned char ch)
{
   putch(ch); 
}

uint8_t serial_getch(void)
{
   return getch(); 
}

uint8_t serial_getch_available()
{
	return circular_buffer_usage(buffer_in);
	
}