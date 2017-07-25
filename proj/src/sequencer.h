#ifndef SEQUENCER_H_
#define SEQUENCER_H_
// This is a x-platform file that lays out memory structure for sequencer
// The globals are defined in sequencer.cpp
#include "protos.h"

//
// Sizes, limits, etc.
//
 
// our fw version number
#define VERSION_NUMBER  1
// Definitions 
#define NUM_STEPS       32
#define DEFAULT_NUM_STEPS 16
#define NUM_TRACKS      6
#define NUM_PATTERNS    4
#define DEFAULT_BPM     120       // until we get clock sync, we assume this bpm, also to start with it will be this
#define MAX_STEP_VELOCITY    15
#define MAX_STEP_PROBABILITY 15
#define DEFAULT_MIDI_NOTE 60

#define TRANSPORT_STOPPED   0
#define TRANSPORT_STARTED   1

#define MODE_NORMAL         0 // turn on and off steps, switch tracks, mute tracks
#define MODE_EDIT_STEP      1 // edit individual steps
#define MODE_PATTERN        2 // set up patterns
#define MODE_GLOBAL         3 // set global settings

#ifdef _WIN32
#pragma pack(push, 1)   
#endif
//
// DATA STATE
// Represents user editable parameters that are saved with the sequence. Separate from current running state
//
struct SeqStep {
  uint8_t velocity : 4; // 0=off 15=highest ( if<>0, add 1, multiply by 8 and subtract 1)
  uint8_t probability : 4;   // 0-15 where 15 is always
};

struct SeqTrack {
   uint8_t numSteps : 5; // length of track 1-32 (add 1)
   //supported: 1x, 2x, 4x, 1/4x, 1/2x, 3/4x 
   uint8_t clockDividerNum : 2; // numerator for clock divider (0 based so 0=1)
   uint8_t clockDividerDenom : 2; // denominator for clock divider (0 based)
   uint8_t muted : 1;        // muted or not

   uint8_t midiNote : 7; // which midi note it sends
   SeqStep steps[NUM_STEPS];
   uint8_t lastRandom : 4; // 0..15 current random value for track
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
   int8_t  swing;              // move back only 0..100 where 100 is on the next step
   int8_t  randomRegen;        // generate a new random number every n steps (1..32)
   int8_t  delayPatternSwitch; // whether to delay switching patterns till end of pattern
   uint8_t midiSendClock;      // whether we are sending clock
   uint8_t midiThru;           // whether we are doing thru
};

extern SequencerState gSeqState;

// use this to figure out size at compile time (currently 835)
//char (*__kaboom)[sizeof( gSeqState )] = 1;

#define SLAVE_MODE() (gSeqState.tempo==0)

//
// RUNNING STATE
//
// Data that represents current state such as where we are in the sequence
//

struct TrackRunningState {
   uint8_t position : 5;       // which step is next (only need 5 bit)
   // the reason these two have more bits than is needed is because we need to be able to have
   // as state where we know their cycle is complete
   uint8_t numCycle : 3;       // incremented whenever we schedule a step when clockDiv is 4x, 2x or 3/4                               
   uint8_t denomCycle : 3;     // incremented whenever we get a clock step when clockDiv is 1/4, 1/2 or 3/4
   uint8_t isScheduled : 1;    // whether or not we have scheduled the next note (reset to 0 when it's been played)
   
   uint16_t nextScheduledStart;// when it's scheduled to play
};

struct RunningState {
   uint8_t   tempo;      // current actual tempo (calculated or determined from seq state)
   TrackRunningState  trackPositions[NUM_TRACKS]; // which position on each track we played last (only need 5 bits per)

    // which pattern we are playing and editing (could use 2 bits)
   uint8_t   pattern;
   // which pattern needs to be switched to (user request only, not autoswitch)
   // This will be set when the user hits a button. it overrides the auto pattern switch
   // default value is NUM_PATTERNS (since its 0 based)
   // the pattern switch will either occur now, or end of current pattern if set to do so
   uint8_t   nextPattern;
   // nth cycle of current pattern - to determine when to switch (could use 4 bits)
   uint8_t   patternCurrentCycle;
   // state (playing, stopped, paused)
   uint8_t   transportState;
   // if true, hold the current pattern and don't increase patternCurrentCycle
   uint8_t   patternHold;
   uint16_t  lastStepTime;   // when the last whole step got triggered
   uint16_t  lastDivStartTime;  // when the last time we started a div cycle (ie 1/4, 1/2 or 3/4 time cycle)
   uint8_t   lastStepTimeTriggered; // timer will set this, scheduler will clear it (bit)
   uint8_t haveClockTick; // (bit) will be set whenever we get a clock tick (midi or internal)
   uint8_t   patternHasPlayed;      // will be set to true whenever a whole pattern (ie track 1 of a pattern) has played
   // we need it for if tempo changes - we might need to throw away steps

   // UI
   uint8_t   currentMode;    // which mode we are in
   uint8_t   currentTrack;   // which track is currently displayed
   uint8_t   currentStep;    // which step is being edited

};

extern RunningState gRunningState;

#ifdef _WIN32
#pragma pack(pop)
#endif

#endif
