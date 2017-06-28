/************************************************************************
 *   AS - taking this groovesizer golf as a starting point to make the stochastic sequencer 
 *   when I started, global vars took up 1559
 *   See end of this file for some info
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

EEPROM256_512 mem_1;            // define the eeprom chip
byte rwBuffer[64];              // our EEPROM read/write buffer


// Definitions 
#define NUM_STEPS     32
#define NUM_TRACKS    6
#define NUM_PATTERNS  4
#define DEFAULT_BPM   120       // until we get clock sync, we assume this bpm, also to start with it will be this

struct SeqStep {
  uint8_t velocity : 4; // 0=off 15=highest ( if<>0, add 1, multiply by 8 and subtract 1)
  uint8_t probability : 4;   // 0-15 where 15 is always
};

struct SeqTrack {
  uint8_t numSteps :5; // length of track 1-32 (add 1)
  uint8_t clockDivider : 3; //speed 4x 2x 1x 1/2x 1/4x 1/8x
  uint8_t muted : 1;        // muted or not
  uint8_t midiNote : 7; // which midi note it sends
  SeqStep steps[NUM_STEPS]; 
};

struct SeqPattern {
  uint8_t numCycles : 4; // add 1 for 16 - number of times to play
  uint8_t nextPatternProb[NUM_PATTERNS/2]; // 4 bits for probability of which pattern plays next
  SeqTrack tracks[NUM_TRACKS];
};

//
// Data that can be saved/loaded (static during sequencer run operation)
// we have 32k of eeprom so we can store a number of these
//
struct SequencerState {
  SeqPattern patterns[NUM_PATTERNS]; // about 828 bytes
  uint8_t midiChannel;        // which midi channel we are sending on
  uint8_t tempo;              // 0=slave (respond to start/stop hopefully as well?)
  int8_t  swing;              // move forward or backward
  int8_t  randomRegen;        // generate a new random number every n steps (1..32)
  int8_t  delayPatternSwitch; // whether to delay switching patterns till end of pattern
};

SequencerState gSeqState;

//
// Data that represents current state such as where we are in the sequence
//
struct RunningState {
  uint8_t tempo;      // current actual tempo (calculated or determined from seq state)
  uint8_t stepPosition; // which position (0..31) we are on (played last)    (might not need since we are storing per track)
  uint8_t trackPositions[NUM_TRACKS]; // which position on each track we played last (only need 5 bits per)
  // which pattern we are playing and editing (could use 2 bits)
  uint8_t pattern;
  // which pattern needs to be switched to at end of bar? overrides programmed switch
  uint8_t nextPattern;
  // nth cycle of current pattern - to determine when to switch (could use 4 bits)
  uint8_t patternCurrentCycle;  
  // state (playing, stopped, paused)
  uint8_t transportState;
  // if true, hold the current pattern and don't increase patternCurrentCycle
  uint8_t patternHold;
};
RunningState gRunningState;
const uint8_t gVersionNumber = 1;

// sequencer variables
byte seqTrueStep; // we need to keep track of the true sequence position (independent of sequence length)
volatile byte seqCurrentStep; // the current step - can be changed in other various functions and value updates immediately
byte lastStep = 0; // used to blink the LED for thelast step played
char seqNextStep = 1; // change this to -1 to reverse pattern playback
byte seqFirstStep = 0; // the first step of the pattern
byte seqLastStep = 31; // the last step of the pattern
byte seqLength = 32; // the length of the sequence from 1 step to 32

byte autoCounter = 0; // keeps count of the parameter auomation
byte seqStepSelected[4]; // "1" means a step is selected, "0" means it's not 
// each byte corresponds to a row of button/leds/steps, each bit of the byte corresponds to a single step   
// it would have been easier to create an array with 32 bytes, but hey, we only need 4 with a bit of bitwise math
// and since we're already doing bitwise operations to shift in and out, we may as well 
byte seqStepMute[4]; // is the step muted or not; 4 bytes correspond to each of the rows of buttons - "0" is top, "3" is bottom
byte seqStepSkip[4]; // is the step muted or not; 4 bytes correspond to each of the rows of buttons - "0" is top, "3" is bottom

boolean mutePage = true;
boolean skipPage = false;

byte programChange = 3;

volatile boolean seqMidiStep; // advance to the next step with midi clock
byte bpm = 96;
unsigned long currentTime;
unsigned int clockPulse = 0; // keep count of the  pulses
byte swing16thDur = 0; // a swung 16th's duration will differ from the straight one above
byte swing = 0; // the amount of swing from 0 - 255 
boolean seqRunning = true; // is the sequencer running?

byte followAction = 0; // the behaviour of the pattern when it reaches the last step (0 = loop, 1 = next pattern, 2 = return to head, 3 = random chromatic, 4 = random major, 5 = random minor, 6 = random pentatonic)

byte head = 0; // the first in a series of chained patterns - any pattern triggered by hand is marked as the head (whether a followAction is set or not) 

boolean saved = false; // we need this variable for follow actions - only start doing the follow action once the pattern has been saved
// loading a pattern sets this variable to true - setting follow action to 1 or 2 sets this to false  

// we define a struct called Track - it contain 4 bytes for each stepOn, stepAccent, and stepFlam
typedef struct Track {
  byte stepOn[4]; // is the step on?
  byte level; // the MIDI velocity of a normal / non-accented step 
  byte stepAccent[4]; // does the step have an accent?
  byte accentLevel; // the MIDI velocity of an accented step
  byte stepFlam[4]; // does the step have a flam?
  unsigned int flamDelay; // the the distance between flam hits in clock pulses - selected by user
  byte flamDecay; // the amount by which each consecutive flam's velocity is lowered - selected by user
  unsigned long nextFlamPulse; // when the next flam should occur
  byte nextFlamLevel; // what the velocity of the flam will be
  byte midiChannel; // the MIDI channel the track transmits on 
  byte midiNoteNumber; // the note number the track sends
} 
Track;

Track track[12]; // creates an array of 12 tracks

byte fxFlam[12] = 
{
  0, 0, 0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0
};

unsigned int fxFlamDelay = 8; // the delay time for fx flams in clock pulses

char fxFlamDecay; // decay for fx flams (-128 to +128)

byte currentTrack = 0; // the track that's currently selected

byte toc[16] = {
  0, 0, 0, 0,
  0, 0, 0, 0,  // 14 bytes to serve as table of contents (toc) for our 112 memory locations (stored as 14 bytes on eeprom page 448) 
  0, 0, 0, 0,  // add 2 bytes at the end for the sake of simplicity, but their values will never change from 0
  0, 0, 0, 0}; // these are the last two unused rows on the 4th triger page (trigPage = 3)      



int pageNum;                // which EEPROM page we'll read or save to  
boolean recall = false;     // a pattern change was requested with a buttonpress in trigger mode 

boolean clearMem = false; // have we marked a memory location to be erased?

// needed by savePatch()
boolean save = false;
unsigned long longPress = 0;

// how dense are the notes created in the pattern randomizer
byte noteDensity;

byte trigPage = 0; // which trigger page are we on?
byte controlLEDrow = B00000010; 

int pot[6]; // to store the values of out 6 pots
int potLock[6]; // to lock the pots when they're not being adjusted
unsigned long lockTimer;

boolean potTemp; // a temp variable just while we do cleanup

unsigned long buttonTimer; // to display numbers for a period after a button was pressed

unsigned long windowTimer; // to display the playback window for a period after it was adjusted
boolean showingWindow = false; // are we busy displaying numbers

// for the rows of LEDs
// the byte that will be shifted out 
byte LEDrow[5];

// setup code for controlling the LEDs via 74HC595 serial to parallel shift registers
// based on ShiftOut http://arduino.cc/en/Tutorial/ShiftOut
const byte LEDlatchPin = 2;
const byte LEDclockPin = 6;
const byte LEDdataPin = 4;

// setup code for reading the buttons via CD4021B parallel to serial shift registers
// based on ShiftIn http://www.arduino.cc/en/Tutorial/ShiftIn
const byte BUTTONlatchPin = 7;
const byte BUTTONclockPin = 8;
const byte BUTTONdataPin = 9;
//@AS*********************************************************************************************
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

#define BUTTON_CLEAR_JUST_INDICATORS() (memset(gButtonJustChanged,0,BUTTON_BYTES))  
//@AS*********************************************************************************************

// this determines what the buttons (and pots)are currently editing
byte mode = 0;
// 0 = step on/off
// 1 = accents   
// 2 = flams
// 3 = master page
// 4 = trigger mode

byte *edit[4]; // here come the pointers - things are getting a little tricky
// we want to be able to choose which of the byte-arrays in the sequencer struct we want to edit
// sadly, we can't simply assign a whole array to a variable ie. editThis = stepMute[];
// that means we'd have to duplicate a bunch of code
// we can, however, assign the address in memory of a byte in the arrays to a pointer variable eg. *edit
// (the star/asterisk indicates that we're declaring a pointer variable)
// like so: edit1 = &seqStepMute[1] (the & means we're assigning the address of seqStepMute[1], NOT the value is holds)
// when we use the variable again, we'll use an asterik again (dereferencing);
// good info on pointers here http://www.tutorialspoint.com/cprogramming/c_pointers.htm
// and here http://pw1.netcom.com/~tjensen/ptr/ch1x.htm

// 4051 for the potentiometers
// array of pins used to select 1 of 8 inputs on multiplexer
const byte select[] = {
  17,16,15}; // pins connected to the 4051 input select lines
const byte analogPin = 0;      // the analog pin connected to multiplexer output

// variables for changing the voices of each oscilator
//byte voice1 = 0; 
//byte lastVoice1 = 0;

int decay = 100; // like filter decay/release

//byte xorDistortion = 0; // the amount of XOR distortion - set by pot2 in pot-shift mode
byte distortion = 0; // either the XOR distortion amount, or a fixed value for accents
//int xorDistPU = 50; // pickup shouldn't be true at the start


byte nowPlaying = 255; // the pattern that's currently playing. 255 is a value we'll use to check for if no pattern is currently playing
byte cued = 255; // the pattern that will play after this one. 255 is a value we'll use to check for if no pattern is currently cued
byte confirm = 255; // the memory location that will be overwritten - needs to be confirmed. 255 means nothing to confirm
boolean cancelSave = false;


byte bpmTaps[10];

//********* UI **********
boolean shiftL = false;
boolean shiftR = false;
boolean shiftPot = false; // so we can do pots with shift
int longPressDur = 300; // the expected duration of a long press in milliseconds




//***** PREFERENCES *****
//preferences - these are saved on the same EEPROM page (448) as the toc
byte thruOn; // echo MIDI data received at input at output: 0 = off, 1 = full 2 = DifferentChannel - all the messages but the ones on the Input Channel will be sent back (our Input Channel is Omni, so all note messages will be blocked, but clock will pass through).
boolean midiSyncOut; // send out MIDI sync pulses
boolean midiTrigger; // send out trigger messages on channel 10 for slaved Groovesizers
byte triggerChannel = 16;

boolean showingNumbers = false; // are we busy displaying numbers
byte number; // the value that will be displayed by the showNumber() function;

//***** MIDI *****
unsigned long lastClock = 1; // when was the last time we received a clock pulse (can't be zero at the start)
boolean midiClock = false; // true if we are receiving midi clock
boolean syncStarted = false; // so we can start and stop the sequencer while receiving clock
boolean clockStarted = false; // are we receiving clock and we've had a start message? 
unsigned long clockCounterSlave = 0;
unsigned long nextStepIncomingPulse = 0;
byte incomingSwingDur = 0;
byte clockDivSlaveSelect = 1; // default is 16th
byte clockDivSlave[3] = {3, 6, 12};
// unsigned long barTarget = 0; // needed to make sure we keep sync over div changes (pattern changes) when on internal clock  

// create instance of midi:MidiInterface<HardwareSerial) giving it the name midiA on port 'Serial'
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, midiA);

//***** INTERNAL CLOCK *****
unsigned long clockCounter = 0;
byte clockDivSelect = 0; // default is 16th
byte clockDiv[3] = {24, 16, 12}; // the duration of a step in pulses - 16th, 16tr, 32nd
unsigned long nextStepPulse = 0; // the pulse on which we should play the next step 
boolean goLoad = false;
boolean startSeq = false; // a flag to delay starting the sequencer so it aligns with a MIDI clock pulse



/* U I
 *  We can be playing in any mode
 *  A pattern will be considered ended when the longest track has run it's course
 *  
 *  __Normal mode:
 *  -Neither L-shift nor R-shift are lit
 *  -buttons turn on and off notes
 *  -F-buttons switch between the 6 tracks
 *  -Pot 1 length in steps of current track
 *  -Pot 2 clock divider for current track
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
 *  Light F5 indicates pattern hold
 *  Light F6 if lit means delay switch till end of pattern
 *  -L-Shift enters pattern mode (and again exits it)
 *  -F1-F4 select current pattern which will switch right away or after current pattern is done depending on setting (if playing)
 *  -Pot 5 sets number of times to play pattern
 *  -Pot 1-4 set probability of next pattern and is shown as a scale on rows 1-4
 *  -F6 set whether to switch patterns immediately or after current pattern end (lit means delayed switch)
 *  -F5 pattern hold - holds the current pattern (toggle) so that it doesn't switch
 *  __Global mode:
 *  -L-shift and R-shift enter global mode
 *  -Both L and R shift are lit
 *  -Pot 1 - midi channel
 *  -Pot 2 - tempo
 *  -Pot 3 - swing
 *  -Pot 5 - Random regen (how often to regenerate a random number -- every step, every n steps)
 *  -F1 plus another cell will save to that location
 *  -F2 plus another cell will load from that location
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
