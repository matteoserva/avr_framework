#ifndef _TIMER_H
#define _TIMER_H

#include <stdint.h>


void timer_init(uint8_t period_ms);
uint16_t timer_get_elapsed();
uint16_t timer_wait();
uint16_t timer_get_remaining();






#endif