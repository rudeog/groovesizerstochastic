#ifndef AVR_MAIN_H_
#define AVR_MAIN_H_
#ifdef _WIN32
#error AVR specific code should not be included
#endif

#include <Arduino.h>
#include <MIDI.h>
#include <midi_Defs.h>
#include <midi_Message.h>
#include <midi_Namespace.h>
#include <midi_Settings.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/pgmspace.h>       // included so we can use PROGMEM
#include <Wire.h>               // I2C library (required by the EEPROM library below)
#include <EEPROM24LC256_512.h>  // https://github.com/mikeneiderhauser/I2C-EEPROM-Arduino
#include <TimerOne.h>

#include "avr_structs.h"
#include "sequencer.h"
#include "protos.h"


#endif
