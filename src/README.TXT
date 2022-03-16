-------------------------------------------------------------------------------
- Amiga keyboard controller                                                   -
- author: Henryk Richter <henryk.richter@gmx.net>                             -
-------------------------------------------------------------------------------

This is the AT90USB program for a custom Amiga keyboard I was building. The
keyboard matrix matches the Mitsumi layout. It is active low. Therefore, the 
6 outputs are on by default and pulled down one-by-one in the scanning process. 

I've included UART for debugging purposes. Please note that the uart outputs
are for testing and therefore print only approximate labels you find on the keys.
Also, the UART is connected to KBCLOCK and KBDATA lines. With debug on, please
connect the FT232RL to KBCLOCK/KBDATA and use IN3/IN4 for Amiga communication.

Consequently, IN3/IN4 inputs are not available while debugging.

