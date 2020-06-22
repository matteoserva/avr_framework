
##########------------------------------------------------------##########
##########              Project-specific Details                ##########
##########    Check these every time you start a new project    ##########
##########------------------------------------------------------##########

MCU   = atmega644
CPU_MHZ = 8
BAUD  = 9600UL
SERIALPORT = /dev/ttyUSB0
REMOTEHOST = 192.168.0.70

## Also try BAUD = 19200 or 38400 if you're feeling lucky.

## A directory for common include files and the simple USART library.
## If you move either the current folder or the Library folder, you'll 
##  need to change this path to match.


##########------------------------------------------------------##########
##########                 Programmer Defaults                  ##########
##########          Set up once, then forget about it           ##########
##########        (Can override.  See bottom of file.)          ##########
##########------------------------------------------------------##########

PROGRAMMER_TYPE = usbtiny
# extra arguments to avrdude: baud rate, chip type, -F flag, etc.
PROGRAMMER_ARGS = 	

##########------------------------------------------------------##########
##########                  Program Locations                   ##########
##########     Won't need to change if they're in your PATH     ##########
##########------------------------------------------------------##########

CC = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
AVRSIZE = avr-size
AVRDUDE = avrdude

##########------------------------------------------------------##########
##########                   Makefile Magic!                    ##########
##########         Summary:                                     ##########
##########             We want a .hex file                      ##########
##########        Compile source files into .elf                ##########
##########        Convert .elf file into .hex                   ##########
##########        You shouldn't need to edit below.             ##########
##########------------------------------------------------------##########

## Or name it automatically after the enclosing directory
TARGET = $(lastword $(subst /, ,$(CURDIR)))

# Object files: will find all .c/.h files in current directory
#  and in LIBDIR.  If you have any other (sub-)directories with code,
#  you can add them in to SOURCES below in the wildcard statement.
SOURCES=$(wildcard *.c src/*.c *.cpp src/*.cpp)
OBJECTS=$(patsubst %.cpp,%.o,$(SOURCES:.c=.o))



LIBDIRS=$(wildcard ../lib*)
LIBFILENAMES=$(join $(LIBDIRS),$(patsubst ../lib%,/lib%.a,$(LIBDIRS)))

EXTRALIBS=$(patsubst ../lib%,-l %,$(LIBDIRS))
HEADERS=$(wildcard *.h src/*.h include/*.h $(patsubst %,%/include/*.h,$(LIBDIRS)))
EXTRAINCLUDEDIRS=$(patsubst %,-I %,$(sort $(dir $(HEADERS))))

F_CPU=$(addsuffix 000000UL,$(CPU_MHZ))
LIBDIRSFLAGS=$(patsubst %,-L %,$(LIBDIRS))

SUBDIRS=$(patsubst %/,%,$(sort $(dir $(wildcard */))))
SUBIDRS_WITH_MAKE=$(dir $(wildcard */Makefile))
IS_TOP_MAKEFILE=$(and  $(filter 0,$(words $(SOURCES)) ),$(filter-out 0,$(filter $(words $(SUBIDRS_WITH_MAKE)),$(words $(SUBDIRS)))))

## Compilation options, type man avr-gcc if you're curious.
COMMONFLAGS = -Os -g  -Wall -DF_CPU=$(F_CPU) -DBAUD=$(BAUD) $(EXTRAINCLUDEDIRS)
CFLAGS =  -std=gnu99 $(COMMONFLAGS)

CPPFLAGS = -std=c++11 $(COMMONFLAGS)


LDFLAGS = $(LIBDIRSFLAGS) $(EXTRALIBS)
## Optional, but often ends up with smaller code
LDFLAGS += 

TARGET_ARCH = -mmcu=$(MCU)

.PHONY: uploadavrisp uploadserial uploadnet uploadnetisp uploadgpio $(LIBFILENAMES) selectspiport subdirs $(SUBDIRS) subdirs-clean
.SUFFIXES:

DEFAULT_TARGET = $(if $(IS_TOP_MAKEFILE), subdirs,$(if $(filter lib%,$(TARGET)),lib,hex))
CLEAN_TARGET = $(if $(IS_TOP_MAKEFILE), subdirs-clean,clean-single)


all: $(DEFAULT_TARGET)

hex: $(TARGET).hex 

lib: $(TARGET).a

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

subdirs-clean:
	for dir in $(SUBDIRS); do \
	$(MAKE) -C $$dir clean; \
	done


## Explicit pattern rules:
##  To make .o files from .c files 


%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS)  $(TARGET_ARCH)  -c -o $@ $<;

%.o: %.cpp $(HEADERS) Makefile
	$(CC) $(CPPFLAGS) $(TARGET_ARCH)  -c -o $@ $<;

$(TARGET).elf: $(OBJECTS) $(LIBFILENAMES)
	$(CC) $(TARGET_ARCH) $^  -o $@ $(LDFLAGS) 

%.hex: %.elf size
	 $(OBJCOPY) -j .text -j .data -O ihex $< $@

%.eeprom: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@ 

%.lst: %.elf
	$(OBJDUMP) -S $< > $@

$(TARGET).a: $(OBJECTS)
	echo $(OBJECTS)
	avr-ar rcs $@ $^
	
$(LIBFILENAMES):
	echo compiling lib $@
	$(MAKE) -C $(dir $@) lib
## These targets don't have files named after them




debug:
	@echo
	@echo "Source files:"   $(SOURCES)
	@echo "MCU, F_CPU, BAUD:"  $(MCU), $(F_CPU), $(BAUD)
	@echo	
	@echo $(SUBDIRS)
	@echo $(SUBIDRS_WITH_MAKE) $(IS_TOP_MAKEFILE)
# Optionally create listing file from .elf
# This creates approximate assembly-language equivalent of your code.
# Useful for debugging time-sensitive bits, 
# or making sure the compiler does what you want.
disassemble: $(TARGET).lst

disasm: disassemble

# Optionally show how big the resulting program is 
size:  $(TARGET).elf
	$(AVRSIZE) -C --mcu=$(MCU) $(TARGET).elf
	$(AVRSIZE) -B --mcu=$(MCU) $(TARGET).elf

clean-single:
	rm -f $(TARGET).elf $(TARGET).hex $(TARGET).obj \
	$(OBJECTS) $(TARGET).d $(TARGET).eep $(TARGET).lst \
	$(TARGET).lss $(TARGET).sym $(TARGET).map $(TARGET)~ \
	$(TARGET).eeprom $(TARGET).a

clean:	$(CLEAN_TARGET)

squeaky_clean:
	rm -f *.elf *.hex *.obj *.o *.d *.eep *.lst *.lss *.sym *.map *~ *.eeprom

##########------------------------------------------------------##########
##########              Programmer-specific details             ##########
##########           Flashing code to AVR using avrdude         ##########
##########------------------------------------------------------##########

	
uploadavrisp: $(TARGET).hex
	bumpSerial; $(AVRDUDE) -P $(SERIALPORT) -c avrisp -p $(MCU) -b 9600 -v -v -U flash:w:$<

uploadserial: $(TARGET).hex
	$(AVRDUDE) -p $(MCU) -c arduino -P $(SERIALPORT)  -b 57600 -U flash:w:$<

upload19ser: $(TARGET).hex
	$(AVRDUDE) -p $(MCU) -c arduino -P $(SERIALPORT)  -b 19200 -U flash:w:$<

uploadnet: $(TARGET).hex
	$(AVRDUDE) -p $(MCU) -c arduino -P net:$(REMOTEHOST):2323 -U flash:w:$<

uploadnetisp: $(TARGET).hex
	$(AVRDUDE) -p $(MCU) -c arduino -P net:$(REMOTEHOST):23  -U flash:w:$<

uploadi2c: $(TARGET).hex
	$(AVRDUDE) -p $(MCU) -c arduino -P $(SERIALPORT)  -b 19200 -U flash:w:$<

uploadgpio: $(TARGET).hex
	@sudo echo 19 > /sys/class/gpio/unexport || true
	@sudo echo 13 > /sys/class/gpio/unexport || true
	@sudo echo 5 > /sys/class/gpio/unexport || true
	@sudo echo 6 > /sys/class/gpio/unexport || true
	sudo avrdude -p atmega644 -C ~/Progetti/avr-scripts/avrdude_gpio.conf -c linuxgpio -i 10 -U flash:w:$<:i
	
SPIPORT=4
selectspiport:
	avrdude -p atmega644 -c arduino -P $(SERIALPORT)   -b 19200 -U eeprom:w:0,0,0,0,$(SPIPORT):m

upload:
	$(MAKE) $(shell echo $$MAKE_AVR_UPLOAD_COMMAND)
	bash -c 'read -t2 -n1 -r -p "Press any key to continue..." key'

uploadtriple:
	$(MAKE) selectspiport SPIPORT=0
	$(MAKE) uploadavrisp
	$(MAKE) selectspiport SPIPORT=1
	$(MAKE) uploadavrisp
	$(MAKE) selectspiport SPIPORT=2
	$(MAKE) uploadavrisp
