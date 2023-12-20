# A500KB
This project contains the material related to a custom Commodore Amiga Mechanical Keyboard. While the initial development target was the iconic Amiga 500, the keyboard also fits in A3000/A4000 (maybe also A2000) external keyboard enclosures. It features the ability to mount Mitsumi or Cherry switches. The Power, Drive and Caps-Lock LEDs are implemented as 7 individually configurable RGB LEDs. LED configuration is provided by an AmigaOS program that talks to the Keyboard over the regular connection. Headers for two additional low active input sources (e.g. HDD, Network) are also present.

The keyboard may be populated with tactile Mitsumi mechanical switches (E25-33-137) and original A2000/3000/4000 keycaps or A1200.net replica keycaps. It has also been tested with Cherry and KailH switches. The author has not reached out to Keycap manufacturers for a fully Amiga compatible keycap set, though.

If building the full keyboard is not an option, a sub-project of A500KB might be of interest. The controller part of A500KB is also usable as standalone controller for classic Mitsumi keyboards (A500KB Mini). While that A500KB Mini approach won't give the benefit of mechanical switches, it'll offer the custom LED functionality of A500KB.

## Requirements

- Amiga 500 or A3000/A4000 keyboard case
- Main PCB and Switchplate PCB
- 96 Keyswitches (Mitsumi or Cherry)
- parts (including AT90USB1287, IS32FL3237, diodes, LEDs and passives)
- suitable LEDs need to be of ARGB type (analog, common anode) such as Würth 150141M173100, Brightek QBLP677-RGB or similar
- 3D Printed parts for LED light guides (transparent) and spacers

## Pictures

![front view](https://github.com/HenrykRichter/A500KB/raw/main/Pics/A500KB_Full.JPG)

![A3000](https://github.com/HenrykRichter/A500KB/raw/main/Pics/A3000KB4.JPG)

## Acknowledgements

Kicad 3D Models and original Footprints imported from:
https://github.com/perigoso/keyswitch-kicad-library

## History

Version 2 of the PCBs addresses several small issues with the prototype. Better fitment is provided for Costar style suports of the larger keys (Shift,Return,Space,Tab etc.). Also, those keys are rotated 90 degrees such that the wire of the supports gets better clearance. The plate PCB got positioning holes for the pins in the top cover of A3000/A4000 keyboards.  Some minor additions were done to the electronics, including larger Diodes and more pullup/down resistors. 

## Caveats

Two styles of A500 top covers need to be considered when it comes to the cabling. 
My prototype features a pin header for the cable to the Amiga. 
While this works for my top case which is smooth underneath, there is another type of case which contains a large "+" style spacer in the area of the keyboard cables. Presumably that "+" was added to keep the keyboard in position.
So to be flexible and on the safe side, I'd suggest to solder the wires towards the Amiga keyboard connector directly into the PCB, just like the original A500 keyboards were done. The external inputs (IN3, IN4) may be provided as pin headers, though.
![plus style spacer](https://github.com/HenrykRichter/A500KB/raw/main/Pics/A500KB_A500Case_potential_Problem.JPG)

## License
The PCB, it's design files and all support code (Amiga, Atmega) are licensed as [CC BY NC SA](https://creativecommons.org/licenses/by-nc-sa/4.0/deed.en).
