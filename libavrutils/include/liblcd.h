#ifndef _LIBI2C_H
#define _LIBI2C_H
#include <stdint.h>

void lcd_init(uint8_t addr);
void lcd_SetCursor(uint8_t row,uint8_t col);


void lcdPrint(char* text);

//void lcd_write(uint8_t* buffer, uint8_t len);
void lcd_pushChar(uint8_t value);

uint8_t lcdFlush();


#endif