#ifndef _CM_SERIAL_H
#define _CM_SERIAL_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

void startSerial(unsigned long baud);
void putch(unsigned char ch);
uint8_t getch(void);

uint8_t serial_putch_available();
uint8_t serial_getch_available();
void serial_putch(unsigned char ch);
uint8_t serial_getch(void);

#ifdef __cplusplus
}
#endif

#endif