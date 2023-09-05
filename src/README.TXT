-------------------------------------------------------------------------------
- Amiga keyboard controller                                                   -
- author: Henryk Richter <henryk.richter@gmx.net>                             -
-------------------------------------------------------------------------------

This is the AT90USB program for a custom Amiga keyboard I was building. The
keyboard matrix matches the Mitsumi layout. It is active low. Therefore, the 
6 outputs are on by default and pulled down one-by-one in the scanning process. 


---
The flashing of the AT90USB1287 is supposed to be done by an external programmer.
An affordable USBASP clone or some FTDI GPIO wiring would be sufficient to serve
that purpose. The pinout of the 6 pin header is the common Atmel ISP.

Three firmwares A500Mini.hex,A500.hex and A3000.hex are present in this folder. 
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
communicationi (Clock/Data).

Consequently, IN3/IN4 inputs are not available while debugging.

