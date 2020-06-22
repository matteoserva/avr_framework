#include <avr/io.h>
#include "analog.h"
#include <avr/interrupt.h>
#include <util/atomic.h>

static uint16_t analog_values[10];
static uint8_t enabled_pins = 0;

#define MUX_BANDGAP 0x1e
#define MUX_0V 0x1f

#define EXTRA_BITS_BASE 2
#define NUM_READS (1<<EXTRA_BITS)
#define DISCARDED_READS_BASE 4


#define ADC_BITS 10

static  uint8_t DISCARDED_READS = DISCARDED_READS_BASE;
static  uint8_t EXTRA_BITS = EXTRA_BITS_BASE;
static uint8_t current_pin = 8;
static uint8_t current_read = 0;
static uint16_t current_value = 0;


enum AnalogMode {MODE_OFF, MODE_AUTO, MODE_MANUAL};

static enum AnalogMode analogMode = MODE_OFF;
/* a 8MHZ/128 = 62k di adc cycle, mi aspetto 558 letture al secondo, 55 giri completi dei 10 canali, un giro ogni 20 milli */

static uint8_t pinToMux(uint8_t pin)
{
	uint8_t returnPin = MUX_BANDGAP;
	
	if(pin < 8)
    {
      returnPin = pin;
    }
    else if(pin == 8)
	{
		returnPin = MUX_0V;
	}
	else
    {
		returnPin = MUX_BANDGAP;
       
    }
	
	return returnPin;
}


ISR(ADC_vect)
{
   
	if(current_read >= DISCARDED_READS)
		current_value += ADC;
	current_read++;
	if(current_read >= ( NUM_READS + DISCARDED_READS) )
	{
		analog_values[current_pin] = current_value<<(16 - (ADC_BITS + EXTRA_BITS));
		current_read = 0;
		current_value = 0;
		current_pin++;
		
		if(current_pin > 9)
		{
			current_pin = 0;
		}
		
		while(current_pin < 8 && 0 == ( enabled_pins & (1<<current_pin)) )
			current_pin++;
		
	}
   
   
	ADMUX = (ADMUX & 0xe0) | ( pinToMux(current_pin)<<MUX0);
    //ADMUX &= ~_BV(ADLAR);
    
    ADCSRA |= (1<<ADSC);
}

void analog_set_extra_bits(uint8_t b)
{
	EXTRA_BITS = b;
	
}

void analog_set_discarded_reads(uint8_t b)
{
	DISCARDED_READS = b;
	
}

static uint8_t analog_calc_prescaler()
{
  uint32_t adc_clock  = F_CPU / 128UL;
  uint32_t adc_prescaler = 7;
  while(adc_clock < 50000UL && adc_prescaler > 0)
  {
    adc_clock *= 2;
    adc_prescaler--;
  }
  adc_prescaler = adc_prescaler << ADPS0;
  return adc_prescaler;
}

void analog_enable(uint8_t pins)
{
  enabled_pins = pins;
  current_pin = 8;
  current_read = 0;
  if(1 || enabled_pins != 0)
  {  
    ADMUX =   _BV(REFS0)  // AVCC with capacitor at AREF 
	    //| _BV(ADLAR) //left adjust result
	    | (MUX_BANDGAP<<MUX0);  
	    
    uint8_t prescalerVal = analog_calc_prescaler();	  
    ADCSRA = _BV(ADEN)| _BV(ADIE) | _BV(ADSC) | prescalerVal;
  }
  else
  {
      ADCSRA = 0;
    
  }
  analogMode = MODE_AUTO;
}

void analog_enable_manual()
{
  analog_enable(0xff); 
  ADCSRA &= ~_BV(ADIE);
 
  analogMode = MODE_MANUAL;
}



uint16_t analog_read_ref_mv()
{
	uint16_t bandgap_raw_val = 0;
	ATOMIC_BLOCK(ATOMIC_FORCEON)
	{
       bandgap_raw_val = analog_values[9];
	}
	
	
	uint32_t bandgap_millivolts = 1069UL << 16;
	bandgap_millivolts /= bandgap_raw_val;
	
	return bandgap_millivolts;
}

uint16_t analog_read_mv(uint8_t pin)
{
	if(analogMode == MODE_MANUAL)
	{
		uint16_t bandgap_raw_val = analog_convert(9);
		
		
		uint16_t pin_raw_val= 0xffff;
		if(pin < 8)
			pin_raw_val = analog_convert(pin);
		uint32_t bandgap_millivolts = 1069UL * pin_raw_val/bandgap_raw_val;
	
		return bandgap_millivolts;
		
		
	}
	
	
	if(pin >= 8)
		return analog_read_ref_mv();
		
	uint16_t bandgap_raw_val = 0;
	uint16_t pin_raw_val = 0;
	ATOMIC_BLOCK(ATOMIC_FORCEON)
	{
       bandgap_raw_val = analog_values[9];
	   pin_raw_val = analog_values[pin];
	}
	
	uint32_t bandgap_millivolts = 1069UL * pin_raw_val/bandgap_raw_val;
	
	return bandgap_millivolts;
}



uint8_t analog_read(uint8_t pin)
{
  uint8_t ret = 0;
  if(pin < 8)
    ret =  analog_values[pin]>>8;
  else
    ret = analog_values[9]>> 8;
  return ret;
}

uint16_t analog_convert_raw(uint8_t pin)
{
	ADMUX = (ADMUX & 0xe0) | (pinToMux(pin) << MUX0) ;
	ADCSRA |= _BV(ADSC) ; 
	while(ADCSRA & _BV(ADSC))
		;
	
	return ADC;
}

uint16_t analog_convert(uint8_t pin)
{
	uint8_t muxOld = ADMUX;
	uint16_t retValue = analog_convert_raw(pin);
	
	if(ADMUX!= muxOld)
	{
		for(int i = 0; i < DISCARDED_READS; i++)
			retValue = analog_convert_raw(pin);
	}
	
	for(int i = 1; i < NUM_READS;i++)
	{
		retValue += analog_convert_raw(pin);
	}
	retValue = retValue<<(16 - (ADC_BITS + EXTRA_BITS));
	
	return retValue;
	
}