# A500KB
This project contains the material related to a custom Commodore Amiga Mechanical Keyboard. While the initial development target was the iconic Amiga 500, the keyboard also fits in A3000/A4000 (maybe also A2000) external keyboard enclosures. It features the ability to mount Mitsumi or Cherry switches. The Power, Drive and Caps-Lock LEDs are implemented as 7 individually configurable RGB LEDs. LED configuration is provided by an AmigaOS program that talks to the Keyboard over the regular connection. Headers for two additional low active input sources (e.g. HDD, Network) are also present.

The keyboard may be populated with two options of Switches and associated Keycaps. The first option are the tactile Mitsumi mechanical switches (E25-33-137) and original A2000/3000/4000 keycaps or A1200.net replica keycaps. Please note that the Mitsumi mechanical switches are quite rare. The second option for switches and keycaps is standard Cherry MX or compatibles. In that latter case, the keycaps for the original Amiga layout are hard to obtain. Recently, the user steveed on [Amibay](https://www.amibay.com/threads/amiga-mx-style-keycaps-pcbs.2446001/) has managed to get replica Amiga compatible keycaps ordered from SignaturePlastics. Consequently, the recent designs of the plate are targeted at Cherry MX compatible stabilizers. The PCB still accepts Mitsumi or Cherry MX (3 pin or 5 pin) compatible switches.

If building the full keyboard is not an option, a sub-project of A500KB might be of interest. The controller part of A500KB is also usable as standalone controller for classic Mitsumi keyboards (A500KB Mini). While that A500KB Mini approach won't give the benefit of mechanical switches, it'll offer the custom LED functionality of A500KB.

Please have a look at the README.md in PCB/ for ordering/building notes.

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

![CheckmateCase](https://github.com/HenrykRichter/A500KB/raw/main/Pics/Checkmate_Case.jpg)


## Acknowledgements

Kicad 3D Models and original Footprints imported from:
https://github.com/perigoso/keyswitch-kicad-library

## History

Version 2 of the PCBs addresses several small issues with the prototype. Better fitment is provided for Costar style suports of the larger keys (Shift,Return,Space,Tab etc.). Also, those keys are rotated 90 degrees such that the wire of the supports gets better clearance. The plate PCB got positioning holes for the pins in the top cover of A3000/A4000 keyboards.  Some minor additions were done to the electronics, including larger Diodes and more pullup/down resistors. Version 4 (see below) addresses potential space issues with A500 cases and cabling headers.

A set of 3D printed parts to fit A500KB into the Checkmate external keyboard case has been added in Apr-2025. Please see [3DPrint](https://github.com/HenrykRichter/A500KB/tree/main/3DPrint/Checkmate_KB_Case) for STL files and guidance pictures.

## Caveats

Potential space issues in different styles of A500 cases affected early versions (up to V3) of the design, where the keyboard could only mounted with soldered cables in most A500 cases. Since V4e of the PCB design, the connector J4 has been moved so that it may be populated with an angled pin header and fit all A500 cases as well as external (A2000,A3000,A4000) cases.

## License
The PCB, it's design files and all support code (Amiga, Atmega) are licensed as [CC BY NC SA](https://creativecommons.org/licenses/by-nc-sa/4.0/deed.en).
