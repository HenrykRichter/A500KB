# Name: Makefile
# Author: <insert your name here>
# Copyright: <insert your copyright message here>
# License: <insert your license reference here>

# This is a prototype Makefile. Modify it according to your needs.
# You should at least check the settings for
# DEVICE ....... The AVR device you compile for
# CLOCK ........ Target AVR clock rate in Hertz
# OBJECTS ...... The object files created from your source files. This list is
#                usually the same as the list of source files with suffix ".o".
# PROGRAMMER ... Options to avrdude which define the hardware you use for
#                uploading to the AVR and the interface where this hardware
#                is connected. We recommend that you leave it undefined and
#                add settings like this to your ~/.avrduderc file:
#                   default_programmer = "stk500v2"
#                   default_serial = "avrdoper"
# FUSES ........ Parameters for avrdude to flash the fuses appropriately.

DEVICE     = at90usb1287
CLOCK      = 16000000
PROGRAMMER = -c usbasp
OBJECTS    = uart.o twi.o main.o usb.o spi.o led_digital.o
OBJECTS3000 = led3000.o $(OBJECTS)
OBJECTS500  = led500.o $(OBJECTS)
OBJECTS500M = led500M.o $(OBJECTS)

DEFS       =
#DEFS       = -DDEBUG
HEADERS	   = kbdefs.h 
FUSES      = -U hfuse:w:0x91:m -U lfuse:w:0xdf:m
# 99/5E are default for ATMegaUSB1287
# DA/FF were what my Atmega 328p's had as default...

# ATMegaUSB1287:
# Example for 16 MHz external oscillator
# Fuse high byte (def:0x99):
# 0x99 = 1 0 0 1   0 0 0 1 <-- BOOTRST (boot reset vector at 0x0000)
#        ^ ^ ^ ^   ^ ^ ^------ BOOTSZ0
#        | | | |   | +-------- BOOTSZ1
#        | | | |   +---------- EESAVE (set to 0 to preserve EEPROM over chip erase)
#        | | | +-------------- WDTON    (watchdog, 1=unprogrammed or not on)
#        | | +---------------- SPIEN    (if set to 1, serial programming is disabled)
#        | +------------------ JTAGEN   (def: 0 = JTAG Enabled)
#        +-------------------- OCDEN    (def: 1 = OCD disabled)
#
# Fuse Low byte (def:0x5E)
# 0xDF = 1 1 0 1   1 1 1 1 
#        ^ ^ \ /   \--+--/
#        | |  |       +------- CKSEL 3..0 (8M internal RC, 1111 external quartz)
#        | |  +--------------- SUT 1..0 (slowly rising power)
#        | +------------------ CKOUT(def 1 unprogrammed)
#        +-------------------- CKDIV8 (def 0 programmed = clock divide by 8, want 1 = no clock div 8)


# ATMega32 fuse bits used above (fuse bits for other devices are different!):
# Example for 12 MHz external oscillator
# Fuse high byte:
# 0xca = 1 1 0 0   1 0 1 0 <-- BOOTRST
#        ^ ^ ^ ^   ^ ^ ^------ BOOTSZ0
#        | | | |   | +-------- BOOTSZ1
#        | | | |   +---------- EESAVE (set to 0 to preserve EEPROM over chip erase)
#        | | | +-------------- CKOPT  (def: 1 for 0.4-8 MHz, set 0 for 1-16 MHz)
#        | | +---------------- SPIEN  (if set to 1, serial programming is disabled)
#        | +------------------ JTAGEN (debug wire, def:0 = enable)
#        +-------------------- OCDEN  (def:1 )
# Fuse low byte:
# 0xEF = 1 1 1 0   1 1 1 1
#        ^ ^ \ /   \--+--/
#        | |  |       +------- CKSEL 3..0 (1M internal RC 0001, 1111 external quartz)
#        | |  +--------------- SUT 1..0 (slowly rising power)
#        | +------------------ BODEN (1=unprogrammed)
#        +-------------------- BODLEVEL (1=unprogrammed, 
#
# For computing fuse byte values for other devices and options see
# the fuse bit calculator at http://www.engbedded.com/fusecalc/


# Tune the lines below only if you know what you are doing:

AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE)
COMPILE = avr-gcc -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE) $(DEFS)
COMPILEPP = avr-g++ -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE) $(DEFS)


# symbolic targets:
all:	AtmegaBoard.hex A3000.hex A500.hex A500Mini.hex A500A3000Uni.hex

.c.o:	$(HEADERS) kbdefs.h
	$(COMPILE) -c $< -o $@

.cpp.o:	$(HEADERS)
	$(COMPILEPP) -c $< -o $@

main.o: main.c $(HEADERS)
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

.c.s:
	$(COMPILE) -S $< -o $@

# defaults to AtmegaBoard.hex which is identical to A500A3000Uni.hex
flash:	all
	$(AVRDUDE) -U flash:w:AtmegaBoard.hex:i

fuse:
	$(AVRDUDE) $(FUSES)

# Xcode uses the Makefile targets "", "clean" and "install"
install: flash fuse

# if you use a bootloader, change the command below appropriately:
load: all
	bootloadHID AtmegaBoard.hex

clean:
	rm -f AtmegaBoard.hex A500A3000Uni.hex main.elf $(OBJECTS) led.o
	rm -f A3000.hex A500.hex A500mini.hex led500.o led500M.o led3000.o main3000.elf main500.elf main500M.elf



led3000.o:	$(HEADERS) kbdefs.h led.c
	$(COMPILE) -DKEYBOARD_TYPE=LEDGV_TYPE_A3000 -c led.c -o $@

led500.o:	$(HEADERS) kbdefs.h led.c
	$(COMPILE) -DKEYBOARD_TYPE=LEDGV_TYPE_A500 -c led.c -o $@

led500M.o:	$(HEADERS) kbdefs.h led.c
	$(COMPILE) -DKEYBOARD_TYPE=LEDGV_TYPE_A500Mini -c led.c -o $@

# file targets:
main.elf: $(OBJECTS) led.o
	$(COMPILE) -o main.elf led.o $(OBJECTS)

main3000.elf: $(OBJECTS3000)
	$(COMPILE) -o $@ $(OBJECTS3000)

main500.elf: $(OBJECTS500)
	$(COMPILE) -o $@ $(OBJECTS500)

main500M.elf: $(OBJECTS500M) led.o
	$(COMPILE) -o $@ $(OBJECTS500M)

A3000.hex:	main3000.elf
	rm -f $@
	avr-objcopy -j .text -j .data -O ihex main3000.elf $@
#	avr-size --format=avr --mcu=$(DEVICE) main3000.elf

A500.hex:	main500.elf
	rm -f $@
	avr-objcopy -j .text -j .data -O ihex main500.elf $@
#	avr-size --format=avr --mcu=$(DEVICE) main500.elf

A500Mini.hex:	main500M.elf
	rm -f $@
	avr-objcopy -j .text -j .data -O ihex main500M.elf $@
#	avr-size --format=avr --mcu=$(DEVICE) main500M.elf

A500A3000Uni.hex:	main.elf
	rm -f $@
	avr-objcopy -j .text -j .data -O ihex main.elf $@
#	avr-size --format=avr --mcu=$(DEVICE) main.elf

AtmegaBoard.hex: main.elf
	rm -f AtmegaBoard.hex
	avr-objcopy -j .text -j .data -O ihex main.elf AtmegaBoard.hex
	avr-size --format=avr --mcu=$(DEVICE) main.elf
# If you have an EEPROM section, you must also create a hex file for the
# EEPROM and add it to the "flash" target.

# Targets for code debugging and analysis:
disasm:	main.elf
	avr-objdump -d main.elf

cpp:
	$(COMPILE) -E main.c
