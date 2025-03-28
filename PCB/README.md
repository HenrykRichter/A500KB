# A500KB Building Notes
This project contains the material related to a custom Commodore Amiga Mechanical Keyboard. While the initial development target was the iconic Amiga 500, the keyboard also fits in A3000/A4000 (maybe also A2000) external keyboard enclosures. It features the ability to mount Mitsumi or Cherry switches. The Power, Drive and Caps-Lock LEDs are implemented as 7 individually configurable RGB LEDs. LED configuration is provided by an AmigaOS program that talks to the Keyboard over the regular connection. Headers for two additional low active input sources (e.g. HDD, Network) are also present.

The two variants of the PCBs in this repository are discussed below. In a nutshell: order PCB and plate for the full keyboard or just the A500KBMini PCB for the standalone controller.

## A500KB (full)

### PCB ordering

- 1 PCB from A500KB\_Gerber, 2 layer PCB in FR4, 1.2mm thickness for a little more space in bottom case (standard 1.6mm will still be compatible)
- 1 PCB from A500KBPlate\_Gerber, 1 layer PCB in FR4 or Alu, 1mm thickness for PCB or plate mounted MX stabilizers _OR_ A500KBPlate\_Gerber\_V2D.zip for Costar style stabilizers

### 3D printing

The 3D printed parts are in the 3DPrint/ subfolder. Which of those files are to be printed depends on the target enclosure. Please note that bolts and nuts are required for mounting. 
The keyboard framing spacers are designed for countersunk M2.5 bolts with a total length of 5mm. 
The LED frame is to be mounted by flathead M2 bolts with a total length of 5mm and matching nuts.

As of A500KB V4, the 3D printed parts have been unified. The side spacers will fit A500 (and reproduction cases), as well as at least most of the external keyboard cases (tested: A2000,A3000,A4000 Mitsumi cases).

Four parts are designed for all keyboard types: 
- Keyboard\_Spacer\_L\_F2025  - left side of keyboard
- Keyboard\_Spacer\_R\_F2025  - right side of keyboard
- Keyboard\_Spacer\_VL\_F2025 - front left below spacebar
- Keyboard\_Spacer\_VR\_F2025 - front right below cursor keys

A500
- Keyboard\_LEDs\_Rahmen - frame for holding the LED lenses
- Keyboard\_LEDs - LED lenses to be printed (twice!) in transparent filament or resin. Keep infill low.


You also might consider to look at Spacebar\_ExtraSprings for Mitsumi mechanical based builds. This part allows to place additional springs under the space bar in the same way that was present with Mitsumi Amiga keyboards. 

Also of relevance for Mitsumi builds is Keyboard\_CapsLockLED to be printed in transparent filament or resin. This light guide is supposed to point towards the clear dot on the CapsLock key from underneath.

### Parts

- Please see schematics and/or A500KB.csv for the latest list. All needed components were marked active at the last check with common distributors (like Digikey or Mouser).
- The 7 RGB LEDs need to be of RGBA type (i.e. common Anode) and can be mounted as PLCC or with leads.
- Choose between D1 and D69, depending on switches and keycaps. For CherryMX switches (with SMD LED provision), D69 would be a good choice. D1 is better suited for Mitsumi switches and associated keycaps.
- C15/C16 are not needed.
- The 1x31 flat cable connector (J1) is not needed and stays unpopulated
- Screw-In PCB mounted stabilizers are needed for Cherry MX and compatible
- see below for a description in which cases R6/R10/R11 need to be installed

### Building

- Solder the SMD components first. 
- Populate either D1 or D69 for CapsLock LED, not both. D2 as classic CapsLock LED is optional. D2 can also be disabled in firmware, if desired (i.e. populate R10 to disable D2).
- Install R6 with 390R for use of RGB LEDs at the positions of Power LED and Floppy LED. Leave R6 open when placing classic Commodore A500 Green/Orange or Red/Green LEDs.
- V3b/V4: install R11 when digital LEDs (SK9822 or APA102) are mounted to D70-D84. Leave R11 open, otherwise

After placing the SMD components, flash the ATMega firmware and verify that the keyboard PCB works.
- Option A) connect by USB to some PC and shorten the pins of the individual keys
- Option B) add A500 cable to J4 and test with AmigaTestKit or similar

Please note that diode reworking after the plate was placed and keyswitches were soldered is not a job I'd look forward to. Therefore, diode soldering and orientation really should be verified before going further.

For CherryMX based builds, place the PCB mounted stabilizers including their wires in the next step. Screw-In type stabilizers should be preferred. 8U stabilizer wires are not commonly available. Hence, it might be necessary to custom-bend the spacebar wire (133.4mm wide, 1.5-1.6mm diameter).

Afterwards, the keyswitches may be added. It is suggested to place four keyswitches in the corners and one in the middle at first. This tends to help aligning the plate over the main PCB. 

Add the 3D printed spacers, LED frame and lenses.

Consider some electric short protection as the last step after fitment tests are done and possibly remaining THT parts (like J5) are in. For me, two layers of Kapton (or generic Polyimide) tape have been working well. The Kapton doesn't add significant thickness to the main PCB.


## A500KBMini

A500KBMini is a little less demanding in the build procedure. There is, however a showstopper for prospective builds. None of the shops I am aware of carries the 1x31 angled FFC connector to be required for A500 Mitsumi Keyboards. Scavenging from broken Mitsumi keyboards might be an option.

### PCB ordering

- 1 PCB from A500KBMini\_Gerber, 2 layer PCB in FR4, 1mm-1.6mm thickness 

### 3D Printing
- Keyboard\_LEDs\_Rahmen as frame for holding the LED lenses
- Keyboard\_LEDs LED lenses to be printed (twice!) in transparent filament or resin

### Parts

- Please see schematics and/or A500KB.csv for the latest list. Exclude the 58 Diodes used for NKRO on the full keyboard. Except from the FFC connector, all other needed components were marked active at the last check with common distributors (like Digikey or Mouser).
- The 6 RGB LEDs need to be of RGBA type (i.e. common anode) and can be mounted as PLCC or with leads.
- C15/C16 are not needed.
- The 1x31 flat cable connector (J1) needs to be obtained and fitted.
- R10/R11 stay unpopulated

### Building

- Solder the SMD components first. 
- Install R6 with 390R for use of RGB LEDs at the positions of Power LED and Floppy LED. Leave R6 open when placing classic A500 Green/Orange or Red/Green LEDs.

After placing the SMD components, flash the ATMega firmware and verify that the keyboard PCB works.

Add the 3D printed LED frame and lenses.


