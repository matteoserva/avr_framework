#include "liblcd.h"
#include "i2clib.h"
#include <util/delay.h>
#include <avr/pgmspace.h>

#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// flags for backlight control
#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00

#define En 0x4  // Enable bit
#define Rw 0x2  // Read/Write bit
#define Rs 0x1  // Register select bit

#define LCD_DISPLAYCONTROL 0x08


static uint8_t lcdAddr = 0;
uint8_t _backlightval = LCD_BACKLIGHT;

struct CommandBuffer_t
{
	uint8_t position;
	uint8_t value;
};

#define COMMAND_BUFFER_SIZE 32
#define CHAR_BUFFER_SIZE 64
#define OUTPUT_BUFFER_SIZE 64

struct CommandBuffer_t command_buffer[COMMAND_BUFFER_SIZE];
static uint8_t command_buffer_start = 0;
static uint8_t command_buffer_end = 0;

static uint8_t char_buffer[CHAR_BUFFER_SIZE];
static uint8_t char_buffer_start = 0;
static uint8_t char_buffer_end = 0;



static uint8_t output_buffer[OUTPUT_BUFFER_SIZE];
static volatile uint8_t output_buffer_len = 0;

enum LcdState_t {LCD_IDLE,LCD_TERMINATING};
static volatile enum LcdState_t  lcdState = LCD_IDLE;
 
//const PROGMEM  uint16_t charSet[]  = { 65000, 32796, 16843, 10, 11234}; 
 
 
static void fill4bits(uint8_t value)
{
	output_buffer[output_buffer_len++] = value;
	output_buffer[output_buffer_len++] = value| En;
	output_buffer[output_buffer_len++] = value & ~En;
	
}

static void fillFromCouple(uint8_t value,uint8_t mode)
{
	uint8_t highnib=value&0xf0;
	uint8_t lownib=(value<<4)&0xf0;
	uint8_t char1 = (highnib)|mode| _backlightval;
	uint8_t char2 = (lownib)|mode| _backlightval;
	
	fill4bits(char1);
	fill4bits(char2);
}

static void fillFromChar()
{
	fillFromCouple(char_buffer[char_buffer_start],Rs);
	char_buffer_start++;
	char_buffer_start %= CHAR_BUFFER_SIZE;
}

static void fillFromCommand()
{

		fillFromCouple(command_buffer[command_buffer_start].value,0);
		command_buffer_start++;
		command_buffer_start %= COMMAND_BUFFER_SIZE;
}

static void fillOutputBuffer()
{
	
	if(lcdState == LCD_IDLE)
	{
		while(output_buffer_len + 6 <= OUTPUT_BUFFER_SIZE)
		{
			
			if(command_buffer_start != command_buffer_end && command_buffer[command_buffer_start].position == char_buffer_start)
			{
				fillFromCommand();
				continue;
			}
			
			if(char_buffer_start != char_buffer_end)
				fillFromChar();
			else
				break;
			
		}
	}
}
void lcd_pushChar(uint8_t value)
{
	fillOutputBuffer();
	char_buffer[char_buffer_end] = value;
	char_buffer_end++;
	char_buffer_end %= CHAR_BUFFER_SIZE;
}

static void pushCommand(uint8_t value)
{
	fillOutputBuffer();
	command_buffer[command_buffer_end].position = char_buffer_end;
	command_buffer[command_buffer_end].value = value;
	command_buffer_end ++;
	command_buffer_end %= COMMAND_BUFFER_SIZE;
}



static void lcdTransferCallback(enum i2c_TransferType_t type, enum i2c_TransferPhase_t phase, unsigned int transferred)
{
	if(phase == I2C_TRANSFER_COMPLETE)
	{
		lcdState = LCD_IDLE;
		i2c_master_set_callback(0);
		

		
		if(transferred >= output_buffer_len)
		{
			output_buffer_len = 0;
			fillOutputBuffer();
			if(output_buffer_len > 0)
			{
				i2c_set_buffer(I2C_MASTER_TRANSMIT,output_buffer,output_buffer_len);
				i2c_master_set_callback(lcdTransferCallback);
				if(i2c_master_transmit(lcdAddr))
					lcdState = LCD_TERMINATING;
			}
		}

	}
	
}




uint8_t lcdFlush()
{
	/* lo stato pari a 1 significa che devi ancora richiamarlo */

	if(lcdState == LCD_IDLE)
	{
		
		fillOutputBuffer();
		if(i2c_master_transfer_completed() && output_buffer_len > 0)
		{
			i2c_set_buffer(I2C_MASTER_TRANSMIT,output_buffer,output_buffer_len);
			i2c_master_set_callback(lcdTransferCallback);
			lcdState = LCD_TERMINATING;
			if(!i2c_master_transmit(lcdAddr))
				lcdState = LCD_IDLE;
		}
		
	}
	
	return (lcdState != LCD_IDLE) || output_buffer_len > 0;

}



void lcd_SetCursor(uint8_t row,uint8_t col){
	int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
	pushCommand(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

void lcdPrint(char* text)
{
	int i=0;
	while(*text != 0)
	{
		i++;
		lcd_pushChar(*text);
		
		text++;
	}
	
	while(i < 20)
	{
		lcd_pushChar(' ');
		
		i++;
	}
	
	
}

void lcd_init(uint8_t addr)
{
	lcdAddr = addr;
	
	_delay_ms(50);
	output_buffer_len = 0;
	output_buffer[output_buffer_len++] = _backlightval;
	while(lcdFlush())
		;
	
	_delay_ms(5);

	// cerco di metterlo prima in modo 8-bit, poi gli do il comando per i 4-bit

	// we start in 8bit mode, try to set 4 bit mode
	fill4bits(0x03 << 4);
	while(lcdFlush())
		;
	_delay_us(4500); // wait min 4.1ms

	// second try
	fill4bits(0x03 << 4);
	while(lcdFlush())
		;
	_delay_us(4500); // wait min 4.1ms

	// third go!
	fill4bits(0x03 << 4);
	while(lcdFlush())
		;
	_delay_us(150);

	// finally, set to 4-bit interface
	
	fill4bits(0x02 << 4);
	uint8_t _displayfunction  = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;
	pushCommand(LCD_FUNCTIONSET | _displayfunction);

	uint8_t _displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
	pushCommand(LCD_DISPLAYCONTROL | _displaycontrol);

	pushCommand(LCD_CLEARDISPLAY);
	while(lcdFlush())
		;
	// la cleardisplay è molto lenta
	_delay_ms(2);
	
	
	
}