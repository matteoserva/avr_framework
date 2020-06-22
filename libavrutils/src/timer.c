#include "timer.h"
#include "milliseconds.h"

/* dipende da milliseconds_init() */

uint8_t timer_period;
uint16_t last_fired;

/* inizializzazione */
void timer_init(uint8_t period_ms)
{
	timer_period = period_ms;
	last_fired = milliseconds();
}

/* millisecondi trascorsi dall'ultimo trigger */
uint16_t timer_get_elapsed()
{
	uint16_t elapsed = milliseconds() - last_fired;
	return elapsed;
}

/* quanto manca al trigger */
uint16_t timer_get_remaining()
{
	uint16_t elapsed = timer_get_elapsed();
	if(  elapsed >= timer_period)
	{
		last_fired += timer_period;
		return 0;
	}
	
	return timer_period-elapsed;
}

/* aspetta e restituisci quanti millisecondi ha aspettato */
uint16_t timer_wait()
{
	uint16_t initial_timer = timer_get_remaining();
	while( timer_get_remaining())
			;
	return initial_timer;
}