/************************************************************************
 *   AS - taking this groovesizer golf as a starting point to make the stochastic sequencer 
 *   when I started, global vars took up 1559
 *   1437
 *   See end of this file for some info
 *   global TODO
 *   - check for initial assignment in functions of locals, some may not be needed
 ************************************************************************/

#include <MIDI.h>
#include <midi_Defs.h>
#include <midi_Message.h>
#include <midi_Namespace.h>
#include <midi_Settings.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/pgmspace.h>       // included so we can use PROGMEM
#include <MIDI.h>               // MIDI library
#include <Wire.h>               // I2C library (required by the EEPROM library below)
#include <EEPROM24LC256_512.h>  // https://github.com/mikeneiderhauser/I2C-EEPROM-Arduino
#include <TimerOne.h>

// our fw version number
#define VERSION_NUMBER  1
// Definitions 
#define NUM_STEPS       32
#define NUM_TRACKS      6
#define NUM_PATTERNS    4
#define DEFAULT_BPM     120       // until we get clock sync, we assume this bpm, also to start with it will be this
#define MAX_STEP_VELOCITY    15
#define MAX_STEP_PROBABILITY 15
#define DEFAULT_MIDI_NOTE 60

#define TRANSPORT_STOPPED   0
#define TRANSPORT_PAUSED    1
#define TRANSPORT_STARTED   2

#define MODE_NORMAL         0 // turn on and off steps, switch tracks, mute tracks
#define MODE_EDIT_STEP      1 // edit individual steps
#define MODE_PATTERN        2 // set up patterns
#define MODE_GLOBAL         3 // set global settings

// *****DATA STATE**************************************************************************
struct SeqStep {
  uint8_t velocity : 4; // 0=off 15=highest ( if<>0, add 1, multiply by 8 and subtract 1)
  uint8_t probability : 4;   // 0-15 where 15 is always
};

struct SeqTrack {
  uint8_t numSteps :5; // length of track 1-32 (add 1)
  uint8_t clockDivider : 3; //0=default (1x) sets speed 0=1,1=2,2=4x or 3=1/2, 4=1/4, 5=1/8x
  uint8_t muted : 1;        // muted or not
  uint8_t midiNote : 7; // which midi note it sends
  SeqStep steps[NUM_STEPS]; 
};

struct SeqPattern {
  uint8_t numCycles : 4; // (0 base..add 1) max 16 - number of times to play
  uint8_t nextPatternProb[NUM_PATTERNS/2]; // 4 bits packed, for probability of which pattern plays next value of 0 means never (current continues to play)
  SeqTrack tracks[NUM_TRACKS];
};

// Data that can be saved/loaded (static during sequencer run operation)
// we have 32k of eeprom so we can store a number of these
//
struct SequencerState {
  SeqPattern patterns[NUM_PATTERNS]; // about 828 bytes
  uint8_t midiChannel;        // which midi channel we are sending on (0 based)
  uint8_t tempo;              // 0=slave (respond to start/stop hopefully as well?)
  int8_t  swing;              // move forward or backward
  int8_t  randomRegen;        // generate a new random number every n steps (1..32)
  int8_t  delayPatternSwitch; // whether to delay switching patterns till end of pattern
  uint8_t midiSendClock;      // whether we are sending clock
  uint8_t midiThru;           // whether we are doing thru
};

SequencerState gSeqState;

// use this to figure out size at compile time (currently 835)
//char (*__kaboom)[sizeof( gSeqState )] = 1;

#define SLAVE_MODE() (gSeqState.tempo==0)

//*****RUNNING STATE**************************************************************************
// Data that represents current state such as where we are in the sequence
//

struct TrackRunningState {
  uint8_t position : 5;       // which step is next (only need 5 bit)
  uint8_t divPosition : 2;    // with clock div=4x each clocked step is divided into 4 parts, used to calculate nextScheduledStart
  uint8_t isScheduled : 1;    // whether or not we have scheduled the next note (reset to 0 when it's been played)
  uint16_t nextScheduledStart;// when it's scheduled to play
};

struct RunningState {
  uint8_t   tempo;      // current actual tempo (calculated or determined from seq state)
  TrackRunningState  trackPositions[NUM_TRACKS]; // which position on each track we played last (only need 5 bits per)
  // which pattern we are playing and editing (could use 2 bits)
  uint8_t   pattern;
  // which pattern needs to be switched to at end of bar? overrides programmed switch
  uint8_t   nextPattern;
  // nth cycle of current pattern - to determine when to switch (could use 4 bits)
  uint8_t   patternCurrentCycle;  
  // state (playing, stopped, paused)
  uint8_t   transportState;
  // if true, hold the current pattern and don't increase patternCurrentCycle
  uint8_t   patternHold;
  uint16_t  lastStepTime;   // when the last whole step got triggered

  // UI
  uint8_t   currentMode;    // which mode we are in
  uint8_t   currentPattern; // which pattern is current
  uint8_t   currentTrack;   // which track is current
  uint8_t   currentStep;    // which step is being edited
  
};
RunningState gRunningState;


//*****BUTTONS *********************************************************************************
// 5 x 8 = 40 --- do the math
#define BUTTON_BYTES 5
uint8_t gButtonJustChanged[BUTTON_BYTES];
uint8_t gButtonIsPressed[BUTTON_BYTES];

#define BUTTON_IS_PRESSED(index) \
  ((gButtonIsPressed[(index)/8]) & (1 << ((index) % 8)))

#define BUTTON_JUST_CHANGED(index) \
  ((gButtonJustChanged[(index)/8]) & (1 << ((index) % 8)))

#define BUTTON_JUST_PRESSED(index) \
  (BUTTON_JUST_CHANGED(index) && BUTTON_IS_PRESSED(index))  
  
#define BUTTON_JUST_RELEASED(index) \
  (BUTTON_JUST_CHANGED(index) && (!BUTTON_IS_PRESSED(index)))    

//****POTS ***************************************************************************************
#define POT_COUNT 6
#define POT_1     0
#define POT_2     1
#define POT_3     2
#define POT_4     3
#define POT_5     4
#define POT_6     5

uint16_t gPotValue[POT_COUNT];  // to store the values of out 6 pots (range 0-1023)
uint8_t gPotChangeFlag;         // determine whether pot has changed
// determines whether a pot's value has just changed
#define POT_JUST_CHANGED(index) \
  (gPotChangeFlag & (1 << (index)))
// use to get the value of the pot  
#define POT_VALUE(index) gPotValue[(index)]  

/* put this down here because arduino sometimes fucks up with concatenating files
 *  U I
 *  We can be playing in any mode
 *  A pattern will be considered ended when the longest track has run it's course
 *  
 *  __Normal mode:
 *  -Neither L-shift nor R-shift are lit
 *  -buttons turn on and off notes
 *  -F-buttons switch between the 6 tracks
 *  -Pot 1 length in steps of current track
 *  -Pot 2 midi note for current track
 *  -Pot 3 clock divider for current track
 *  
 *  __Edit mode:
 *  -R-shift enters cell edit mode (r shift again exits it and returns to normal mode)
 *  -R-shift is lit
 *  -hitting a button lets you make that the current step for editing (flashes)
 *  -Pot 1 adjusts velocity of current step
 *  -Pot 2 adjusts probability of current step
 *  -F1-F6 will mute unmute tracks as shown with leds
 *  
 *  __Pattern mode:
 *  Lights F1-F4 show the current pattern
 *  Light F5 indicates pattern hold (dont switch patterns)
 *  Light F6 if lit means delay switch till end of pattern
 *  -L-Shift enters pattern mode (and again exits it)
 *  -F1-F4 select current pattern which will switch right away or after current pattern is done depending on setting (if playing)
 *  -Pot 5 sets number of times to play pattern
 *  -Pot 1-4 set probability of next pattern and is shown as a scale on rows 1-4
 *  -F6 set whether to switch patterns immediately or after current pattern end (lit means delayed switch)
 *  -F5 pattern hold - holds the current pattern (toggle) so that it doesn't switch
 *  
 *  __Global mode:
 *  -L-shift and R-shift enter global mode
 *  -Both L and R shift are lit
 *  -Pot 1 - midi channel
 *  -Pot 2 - tempo (0 to set ext clock)
 *  -Pot 3 - swing
 *  -Pot 5 - Random regen (how often to regenerate a random number -- every step, every n steps)
 *  -F1 plus another cell will save to that location
 *  -F2 plus another cell will load from that location
 *  -F3 toggle midi thru
 *  -F4 toggle midi send clock
 *  
 *  __All modes:
 *  pot 6 is play/pause/stop - left is stop, right is play, middle is pause
 */

/* S T O R A G E
 * We will have:
 * 1 channel selection
 * tempo selection 0=slave, 1-255=tempo
 * swing
 *4 patterns
 * number of times to play pattern (count) 4bit 1-16
 * probability of next pattern to play 1-4 (After count above), 3 bits each where 0 is never, 7 highest
 * 6 tracks (each track can play one midi note)
 *  length selection for track (1-32 notes in track) 5 bits
 *  clock divider (3 bits) speed 4x 2x 1x 1/2x 1/4x
 *  midi note selection for track
 *  32 steps:
 *   velocity level (0=off, 15=highest) 4bits
 *   probability (1 to 16) 4bits
 *   
 *   */
