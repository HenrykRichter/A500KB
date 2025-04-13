-------------------------------------------------------------------------------
- Amiga keyboard controller                                                   -
- author: Henryk Richter <henryk.richter@gmx.net>                             -
-------------------------------------------------------------------------------

This is the AT90USB program for a custom Amiga keyboard I was building. The
keyboard matrix matches the Mitsumi layout. It is active low. Therefore, the 
6 outputs are on by default and pulled down one-by-one in the scanning process. 

Since V4 of the firmware, the USB port has been enabled in device mode. 
CAUTION: USE ONLY ONE CONNECTION, EITHER USB OR AMIGA. NEVER BOTH AT THE SAME 
         TIME. DON'T SAY YOU'VE NOT BEEN WARNED.

The keyboard will act as a Boot mode USB HID device. Please note the following 
caveats: The "#" key (on German keyboards) and the "\" key will emit different 
scan codes (0x31,0x32) result in the same response ("#") with default keyboard 
layouts on PCs/Macs. The keypad [ and ] keys are transmitted as NumLock and 
ScrollLock (as suggested by the secondary label on the keys). CapsLock,NumLock 
and ScrollLock are reflected by the 3 LEDs of the regular floppy row.

---
The flashing of the AT90USB1287 is supposed to be done by an external programmer.
An affordable USBASP clone or some FTDI GPIO wiring would be sufficient to serve
that purpose. The pinout of the 6 pin header is the common Atmel ISP.

Three firmwares A500Mini.hex,A500.hex and A3000.hex are present in this folder
(up to version 3). In V4, the firmware A500A3000Uni.hex was added that will
auto-detect the connection type at runtime and reports "A500" or "A3000",
respectively. Please note that R17 is required on the PCB. 
All of them are functionally identical and just carry different IDs that will
be reflected in the Amiga configuration software.

Example flash procedure (here using USBASP):

avrdude -c usbasp -p at90usb1287 -U flash:w:A500.hex:i
avrdude -c usbasp -p at90usb1287 -U hfuse:w:0x91:m -U lfuse:w:0xdf:m

---
Debugging of the ATMega program:

I've included UART output for debugging purposes. Due to initial design decisions,
there are some caveats to the debugging process. Debug can be enabled by uncommenting
the following line in the Makefile:
DEFS       = -DDEBUG

Please note that the uart outputs are for testing and therefore print only approximate
labels you find on the keys. Also, the UART is connected to KBCLOCK and KBDATA lines.
With debug on, please connect the FT232RL to KBCLOCK/KBDATA and use IN3/IN4 for Amiga
communicationi (Clock/Data, 9k6 8N1).

Consequently, IN3/IN4 inputs are not available while debugging. 

Debugging connection overview:
Amiga:   IN3 = KBCLOCK, IN4 = KBData, remaining pins as usual
FT232RL: KBCLK=RX->FT232TX, KBDATA=TX->FT232RX

Without debug (default builds)
just connect the keyboard in the same way as classic keyboards
--

--
Software version tags:
Beginning with V5, the presence of the LED strip is included
in the version (Bit0). Therefore, versions come in pairs now.
The indicator for LED strip presence is R11. If populated,
then the strip is assumed to be present.

9/10= clock back to 16 MHz, some code optimization
7/8 = watchdog added, reduced clock to 8 MHz, reduced digital LED brightness 
      in order to keep power consumption in check
5/6 = support for LED strip added to the firmware
4 = USB mode added to firmware 
3 = Added A500KB Mini to build variants
2 = Supports (slowly) animated LED modes
1 = initial Release
