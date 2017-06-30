#pragma once
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef unsigned char uint8_t;
typedef char int8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;


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
   uint8_t numSteps : 5; // length of track 1-32 (add 1)
   uint8_t clockDivider : 3; //0=default (1x) sets speed 0=1,1=2,2=4x or 3=1/2, 4=1/4, 5=1/8x
   uint8_t muted : 1;        // muted or not
   uint8_t midiNote : 7; // which midi note it sends
   SeqStep steps[NUM_STEPS];
};

struct SeqPattern {
   uint8_t numCycles : 4; // (0 base..add 1) max 16 - number of times to play
   uint8_t nextPatternProb[NUM_PATTERNS / 2]; // 4 bits packed, for probability of which pattern plays next value of 0 means never (current continues to play)
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
   uint8_t divComplete : 1;    // if divider is 2x or 4x, this bit will be set after all 2(or 4) steps have played to indicate that 
                               // we don't want to schedule any more until a proper tick comes in (case for tempo getting slower)
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
   uint8_t   lastStepTimeTriggered; // timer will set this, scheduler will clear it (bit)

                                    // UI
   uint8_t   currentMode;    // which mode we are in
   uint8_t   currentPattern; // which pattern is current
   uint8_t   currentTrack;   // which track is current
   uint8_t   currentStep;    // which step is being edited

};

extern RunningState gRunningState;
extern SequencerState gSeqState;

void doSequencer();