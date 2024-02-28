/*
usb.h
*/
#ifndef USB_H
#define USB_H
#include <stdbool.h>
#include <stdint.h>

#define USB_KB_NKEYS 6
extern volatile uint8_t keyboard_pressed_keys[USB_KB_NKEYS];
extern volatile uint8_t keyboard_modifier;
extern volatile uint8_t keyboard_leds;

int usb_init();
uint8_t get_usb_config_status();
int usb_send();
int send_keypress(uint8_t, uint8_t);

#define GET_STATUS 0x00
#define CLEAR_FEATURE 0x01
#define SET_FEATURE 0x03
#define SET_ADDRESS 0x05
#define GET_DESCRIPTOR 0x06
#define GET_CONFIGURATION 0x08
#define SET_CONFIGURATION 0x09
#define GET_INTERFACE 0x0A
#define SET_INTERFACE 0x0B

#define idVendor 0x03eb  // Atmel Corp.
#define idProduct 0x2ffb  // AT90USB1287 DFU Bootloader (This isn't a real product so I don't
          // have legitimate IDs)
#define KEYBOARD_ENDPOINT_NUM 3  // The second endpoint is the HID endpoint

#define CONFIG_SIZE 34
#define HID_OFFSET 18

// HID Class-specific request codes - refer to HID Class Specification
// Chapter 7.2 - Remarks

#define GET_REPORT 0x01
#define GET_IDLE 0x02
#define GET_PROTOCOL 0x03
#define SET_REPORT 0x09
#define SET_IDLE 0x0A
#define SET_PROTOCOL 0x0B

// HID Keyboard
#define SET_LED_NUM_B  0
#define SET_LED_CAPS_B 1
#define SET_LED_SCR_B  2
#define SET_LED_COMP_B 3
#define SET_LED_KANA_B 4

#endif
