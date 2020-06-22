#ifndef _I2C_SERIAL_H
#define _I2C_SERIAL_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void i2cserial_init(uint8_t);
void i2cserial_putch(uint8_t);
uint8_t i2cserial_getch();
uint8_t i2cserial_getch_available();


#ifdef _I2C_SERIAL_LIGHTWEIGHT_

#define i2cserial_init i2cserial_init_light
#define i2cserial_putch i2cserial_putch_light
#define i2cserial_getch i2cserial_getch_light
#define i2cserial_getch_available i2cserial_getch_available_light

void i2cserial_init_light(uint8_t);
void i2cserial_putch_light(uint8_t);
uint8_t i2cserial_getch_light();
uint8_t i2cserial_getch_available_light();




#else

#define i2cserial_init i2cserial_init_full
#define i2cserial_putch i2cserial_putch_full
#define i2cserial_getch i2cserial_getch_full
#define i2cserial_getch_available i2cserial_getch_available_full

void i2cserial_init_full(uint8_t);
void i2cserial_putch_full(uint8_t);
uint8_t i2cserial_getch_full();
uint8_t i2cserial_getch_available_full();


#endif










#ifdef __cplusplus
}
#endif

#endif
