Short:        Configuration tool for A500KB
Author:       Henryk Richter
Uploader:     henryk.richter@gmx.net (Henryk Richter)
Type:         util/misc
Version:      1.7
Architecture: m68k-amigaos >= 3.0.0
Distribution: NoCD

 Introduction
 ------------

 This is the Amiga-side configuration utility for A500KB
 (https://github.com/HenrykRichter/A500KB) and A500KBMini.

 A500KB is a replacement keyboard for Amiga computers, based
 on mechanical switches. A500KBMini is a simplified spin-off
 project that just replaces the keyboard controller on A500
 Mitsumi keyboards instead of the whole keyboard assembly.


 Features
 --------

 Please refer to the project site (see above) for the keyboard
 capabilities. This utility allows to change brightness, colors
 and actions for any of the 7 RGB LEDs that are mounted
 on A500KB. Any LED may get it's individual idle, primary 
 and secondary colors. Furthermore, the relationship between 
 LED and indicator type is no longer fixed so that you could 
 assign any input source to any LED at up to two indicated
 sources per LED.


 Invocation
 ----------

 Make sure, you have at least AmigaOS3.0. This program makes
 use of the color pen feature that was introduced with 
 Kickstart 3.0. Also needed are the Colorwheel.gadget and
 Gradientslider.gadget in SYS:Classes/Gadgets.

 The tool can be started from Workbench, as usual. In case
 of problems at this stage, please verify that the permissions
 are correct (i.e. from CLI: protect A500KBConfig RWED).

 When starting, the tool will try to read the current
 configuration from the A500KB Keyboard. The process usually 
 takes 2-5 seconds. After the config is loaded, the title
 bar of the window will show the keyboard type (by installed
 firmware) and firmware version.

 Depending on the number of available pens (free colors), 
 the tool will open an own Screen. Should the Workbench have
 enough free pens, then the tool opens there.


 Concept and Usage
 -----------------
 
 Click on one of the LEDs in the upper left portion of the
 window to change it's parameters. Please note that the
 Floppy and Power LEDs are segmented into three individual
 LEDs that can be configured as desired.

 In the lower left section of the window is the radio button
 for the LED states. Those three states are:

 Off - LED is idle without active indicator signal
       (typically black or darker for the PowerLED)
 On  - primary Source for which one of the sources
       from the "Display Source" cycle may be selected
 2nd - secondary Source for which one of the sources
       from the "Display Source" cycle may be selected

 If both primary and secondary source are active, then
 the color for the primary source is shown. 

 This way of specifying colors can be used for several
 effects, like inverted operation where the "Off" mode
 could be set for a bright color and the "On"/"2nd" 
 states show darker colors.

 Any change to colors and sources will be applied directly
 in the keyboard, however with a small delay.

 Please note that the changes are non permanent (active
 throughout the current power cycle), until "Save EEPROM" 
 is clicked. 

 There are two menu options "Load Preset" and "Save Preset"
 that can be used to load/save a full color scheme.


