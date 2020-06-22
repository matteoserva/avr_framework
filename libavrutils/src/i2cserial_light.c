
#define _I2C_SERIAL_LIGHTWEIGHT_


#include <util/twi.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "crc16.h"
#include "i2cserial.h"
#include <stdint.h>

volatile uint8_t tx_buf[256];
volatile uint8_t rx_buf[256];

volatile uint8_t tx_read =  0;
volatile uint8_t rx_read =  0;
volatile uint8_t tx_write = 0;
volatile uint8_t rx_write = 0;

static uint8_t remaining;
static uint8_t count;

static uint8_t address = 65;

static volatile uint8_t transmission_running = 0;

enum I2C_Serial_status { I2CS_HEADER, I2CS_PAYLOAD , I2CS_CHECKSUM};
static enum I2C_Serial_status status;
#define printf(...)
/* I2C Clock */
#define I2C_F_SCL 100000UL // SCL frequency
#define I2C_PRESCALER 1
#define TWBR_VAL_BASE  ( ((F_CPU / I2C_F_SCL) / I2C_PRESCALER)/2)
#define TWBR_VAL_OFFSET  ( 16  / 2 )

unsigned int ignore_input = 0;

#define MAX_PAYLOAD_PER_PACKET 16


static uint16_t checksum = 0;
void i2cserial_init_light(uint8_t a)
{
	TWCR = 0;
	TWCR = TWCR; // per pulire l'interrupt
	
	DDRC &= ~(_BV(0) | _BV(1));
	//PORTC |= _BV(0) | _BV(1);

	TWAR = 1;

	uint16_t prescaler_base = TWBR_VAL_BASE;
	uint16_t prescaler_offset = TWBR_VAL_OFFSET;
	uint8_t prescalerVal = 0;

	while ( ( prescaler_base - prescaler_offset) > 255) {
		prescalerVal++;
		prescaler_base /= 4;
	}
	TWBR = (uint8_t)prescaler_base;

	TWSR = (prescalerVal & (_BV(TWPS0) | _BV(TWPS1)));

	TWCR |=  (1<<TWEN)| 1<<TWIE ; // != perché mi ero già preparato il TWEA, (1<<TWINT) | sottinteso
    TWCR |= _BV(TWEA);
	
	
	address = a;
}
static void i2c_notify_putch()
{
	
	while(TWCR & _BV(TWSTO))
		;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		if(transmission_running == 0 && tx_write != tx_read)
		{
			uint8_t control = TWCR;
			control &= ~_BV(TWINT);
			control |= _BV(TWSTA);
			
			transmission_running = 1;
			TWCR = control;
		}
		
	}
	//printf("control %02X, status %02X\n",TWCR,TW_STATUS);
}


static uint8_t i2cserial_tx_bufsize()
{
	uint8_t blen = tx_write- tx_read;
	return blen;
	
}

static uint8_t i2cserial_tx_full()
{

	return i2cserial_tx_bufsize() > 254;
	
}

void i2cserial_putch_light(uint8_t el)
{
	while(i2cserial_tx_full())
		;
	tx_buf[tx_write++] = el;
	i2c_notify_putch();
}

uint8_t i2cserial_getch_available_light()
{
	return (rx_write- rx_read);
}

uint8_t i2cserial_getch_light()
{
	while(!i2cserial_getch_available())
		;
	uint8_t val = rx_buf[rx_read++];
	return val;
}


static void handleTwiInterrupt()
{

	uint8_t control = TWCR;
	
	control &= ~_BV(TWSTA);
	
	
	switch( TW_STATUS) {
	case TW_REP_START:
	case TW_START: {
		status = I2CS_HEADER;
		TWDR = 0;
		checksum = crc16_init();
		
		break;
	}
	/* MASTER TRANSMIT */
	case TW_MT_SLA_ACK:
	{
		count = 1;
		
		TWDR = (address << 1) +1;
		checksum = crc16(checksum,TWDR);
	}
		break;

	case TW_MT_SLA_NACK: // slave non ha nemmeno risposto al suo indirizzo
	{
		//tx_read = tx_write;
				//transmission_running = 0;
		control |= _BV(TWSTO);
		control |= _BV(TWSTA);
	}
	
	break;

	case TW_MT_DATA_ACK:
		if(status == I2CS_HEADER)
		{
			count ++;
			
			switch(count)
			{
				case 1:
				case 2:
				TWDR = (address << 1) +1;
				break;
				case 3:
				{
					remaining = i2cserial_tx_bufsize();
					if(remaining > MAX_PAYLOAD_PER_PACKET)
						remaining = MAX_PAYLOAD_PER_PACKET;
					TWDR=remaining;
				}
				break;
				case 4:
				{
					TWDR = 0;
					status = I2CS_PAYLOAD;
					count = 0;
				}
			}
			checksum = crc16(checksum,TWDR);
			
		}
		else if(status == I2CS_PAYLOAD)
		{
			uint8_t pos = tx_read + count;
			TWDR = tx_buf[pos];
			count++;
			checksum = crc16(checksum,TWDR);
			if(count == remaining)
			{
				status = I2CS_CHECKSUM;
				count = 0;
				checksum = crc16_finalize(checksum);
			}
				
		}
		else
		{
			uint8_t* c = (uint8_t* ) &checksum;
			if(count == 0)
			{
				TWDR = c[count];
			}
			else
			{
				TWDR  = c[count];;
				control &= ~_BV(TWEA);
			}
			count++;
		}
		break;
	case TW_MT_DATA_NACK:
		if(count == 2 && status == I2CS_CHECKSUM)
		{
			tx_read += remaining;
			if(tx_write != tx_read)
			{
				control |= _BV(TWSTA);
				
			}
			else
			{
				
				transmission_running = 0;
			}
		}
		else
			{
				control |= _BV(TWSTA);
				//tx_read = tx_write;
				//transmission_running = 0;
			}
		control |= _BV(TWEA);
		control |= _BV(TWSTO);
	
	break;
		//case TW_MR_ARB_LOST:
	case TW_MT_ARB_LOST:
		control |= _BV(TWSTA);
		
		break;

	case TW_SR_ARB_LOST_GCALL_ACK:
		control |= _BV(TWSTA);
		
	case TW_SR_GCALL_ACK:
		status = I2CS_HEADER;
		count = 0;
		checksum = crc16_init();
		ignore_input = 0;
		
	break;

	case TW_SR_GCALL_DATA_ACK:
		if(ignore_input != 0)
		{
			
		}
		else if(status == I2CS_HEADER)
		{
			count ++;
			switch(count)
			{
				case 1:
					if(TWDR != ( (address << 1) +1))
					{
						control &= ~_BV(TWEA);
						ignore_input = 1;
					}
				break;
				case 2:
					if(TWDR != (address << 1) + 1)
						{
							control &= ~_BV(TWEA);
							ignore_input = 1;
						}
				
				break;
				case 3:
				{
					remaining = TWDR;
				}
				break;
				case 4:
				{
					status = I2CS_PAYLOAD;
					count = 0;
				}
			}
			checksum = crc16(checksum,TWDR);
		}
		else if(status == I2CS_PAYLOAD)
		{
			uint8_t pos = rx_write + count;
			rx_buf[pos] = TWDR;
			count++;
			checksum = crc16(checksum,TWDR);
			if(count == remaining)
			{
				status = I2CS_CHECKSUM;
				count = 0;
				checksum = crc16_finalize(checksum);
			}
				
		}
		else
		{
			uint8_t* c = (uint8_t* ) &checksum;
			if(count == 0)
			{
				if(TWDR == c[0])
					count++;
				
				control &= ~_BV(TWEA);
				
			}
			
		}
		break;

	case TW_SR_GCALL_DATA_NACK:
	{
		uint8_t* c = (uint8_t* ) &checksum;
		if(count == 1 && TWDR == c[1] && status == I2CS_CHECKSUM )
		{
			rx_write += remaining;
			
		}
		if(tx_write != tx_read)
		{
			transmission_running = 1;
			control |= _BV(TWSTA);
		}
		control |= _BV(TWEA);

	}
	break;
	/* SLAVE RECEIVE */
	

	case TW_BUS_ERROR:
		control |= _BV(TWSTO);
		
	
	case TW_SR_STOP:
		control &= ~_BV(TWSTA);
		control  |= _BV(TWEA);
		//tx_read = tx_write;
		transmission_running = 0;
		break;
		/***************************/


		
		
		
		break;
	default:

		break;
	}
	printf("I2CS: %02X, con %02X, D %02X, %04X\n",TW_STATUS,control, TWDR,checksum);
	//printf("control %02X, %02X\n",control,TWDR);
	TWCR = control;
}

ISR(TWI_vect)
{
	handleTwiInterrupt();
}













