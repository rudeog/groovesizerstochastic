#ifndef PROTOS_H_
#define PROTOS_H_
//
// Prototypes. This is x-platform file. 
// All prototypes are here including AVR specific
//

#ifndef _WIN32
// AVR specific 
#include <Arduino.h>
#define LOGMESSAGE(...)
#else
#include <stdlib.h>
#include <string.h>
// Emulation specific
typedef unsigned char   uint8_t;
typedef char            int8_t;
typedef unsigned short  uint16_t;
typedef short           int16_t;
typedef unsigned int    uint32_t;
#define LOGMESSAGE(...) logMessage(__VA_ARGS__)
void logMessage(int level, const char *format, ...);
// emulate some arduino and other functions here
uint32_t millis();

#endif

/////////////////////////
// Clock

// initialize the clock
void clockSetup(void);
// things that need to happen as a consequence of the bpm changing
void clockSetBPM(uint8_t BPM);


/////////////////////////
// MIDI

// initialize midi
void midiSetup(void);
// called from the main loop. read midi port, issue callbacks
void midiUpdate(void);
// send a midi clock pulse
void midiSendClock(void);
// send a midi PLAY
void midiSendStart(void);
// send a midi STOP
void midiSendStop(void);
// send a note message (chan is 1 based)
void midiPlayNote(uint8_t chan, uint8_t note, uint8_t vel);
// Set midi thru on/off depending on the state
void midiSetThru(void);

/////////////////////////
// BUTTONS

void buttonSetup(void);
void buttonsUpdate(void);

/////////////////////////
// LEDS

void ledsSetup(void);
void ledsUpdate(void);
// Display a number on the LEDs for a period of time
void ledsShowNumber(uint8_t number);

/////////////////////////
// POTS
void potsSetup(void);
void potsUpdate(void);

/////////////////////////
// EEPROM
void eepromSetup(void);

/////////////////////////
// UI
void uiAcceptInput(void);


/////////////////////////
// SEQUENCER

// setup the sequencer to initial state
void seqSetup(void);
// called from main loop to see if we need to alter sequencer state
void seqCheckState(void);
// set to playing or stopped see TRANSPORT_ defines
void seqSetTransportState(uint8_t state);
// called both when a midi clock is received as well as an internal clock
void seqClockTick(void);
// set the current BPM. Will also fix the timer for the internal clock.
void seqSetBPM(uint8_t BPM);
// user changes pattern
void seqSetPattern(uint8_t pat);

//////////////////////////
// RANDOM

// generate a random number between 0..max-1
uint8_t rndRandom(uint8_t max);

// seed random generator with a state and a sequence number
void rndSRandom(uint8_t initstate, uint8_t initseq);

#endif
