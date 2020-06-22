#include <inttypes.h>

void analog_enable(uint8_t pins);
void analog_enable_manual();
uint16_t analog_convert(uint8_t pin);
uint8_t analog_read(uint8_t pin);
uint16_t analog_read_mv(uint8_t pin);
void analog_set_extra_bits(uint8_t b);
void analog_set_discarded_reads(uint8_t b);