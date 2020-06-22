#include <stdint.h>
#include "i2cbp.h"

#warning TODO

#define INBUFFERLEN 256

uint8_t outBuffer [32];
uint8_t outBufferLen = 0;
uint8_t inBuffer[INBUFFERLEN];
volatile uint16_t inBuffer_w = 0;
volatile uint16_t inBuffer_r = 0;

static uint8_t address;

static void i2cbp_cb(const struct I2CBP_Header_Full* header, uint8_t* payload)
{
	if(header->info.is_broadcast == 1 && header->src_addr == address ) {
		for(int i = 0; i < header->info.len; i++) {
			inBuffer[inBuffer_w] = payload[i];
			inBuffer_w = (inBuffer_w+1)%INBUFFERLEN;
		}
	}
}

static void i2cbp_tcb(const struct I2CBP_Header* header, enum I2CBP_TransmitResult_t result)
{
  
  
  
}




void i2cserial_init_full(uint8_t a)
{
	address = a;
	i2cbp_init();
	i2cbp_set_addr(address);
	i2cbp_enable_broadcast_address(address);
	i2cbp_set_receive_cb(i2cbp_cb);
  
}



void i2cserial_putch_full(uint8_t el)
{
	outBuffer[outBufferLen++] = el;
}

uint8_t i2cserial_getch_available_full()
{
	return inBuffer_r != inBuffer_w;
}

uint8_t i2cserial_getch_full()
{
	uint8_t val = inBuffer[inBuffer_r];
	inBuffer_r++;
	return val;
}













