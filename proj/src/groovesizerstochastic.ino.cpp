#include <Arduino.h>
#line 1 "E:\\codez\\src\\groovesizerstochastic\\groovesizerstochastic.ino"
#line 1 "E:\\codez\\src\\groovesizerstochastic\\groovesizerstochastic.ino"
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
#line 62 "E:\\codez\\src\\groovesizerstochastic\\buttons.ino"
void buttonsUpdate();
#line 10 "E:\\codez\\src\\groovesizerstochastic\\clock.ino"
void internalClock();
#line 34 "E:\\codez\\src\\groovesizerstochastic\\clock.ino"
void clockSetStarted(void);
#line 42 "E:\\codez\\src\\groovesizerstochastic\\clock.ino"
void clockSetBPM(uint8_t BPM);
#line 33 "E:\\codez\\src\\groovesizerstochastic\\leds.ino"
static void ledsFullUpdate();
#line 190 "E:\\codez\\src\\groovesizerstochastic\\leds.ino"
static void shiftOut(int myLEDdataPin, int myLEDclockPin, byte myDataOut);
#line 238 "E:\\codez\\src\\groovesizerstochastic\\leds.ino"
static void ledsOff(void);
#line 247 "E:\\codez\\src\\groovesizerstochastic\\leds.ino"
static void ledsLightUp(void);
#line 279 "E:\\codez\\src\\groovesizerstochastic\\leds.ino"
void ledsShowNumber(uint8_t number);
#line 366 "E:\\codez\\src\\groovesizerstochastic\\leds.ino"
void ledsUpdate(void);
#line 2 "E:\\codez\\src\\groovesizerstochastic\\mainloop.ino"
void loop();
#line 13 "E:\\codez\\src\\groovesizerstochastic\\midi.ino"
void midiUpdate(void);
#line 18 "E:\\codez\\src\\groovesizerstochastic\\midi.ino"
void midiSendClock(void);
#line 23 "E:\\codez\\src\\groovesizerstochastic\\midi.ino"
void midiSendStart(void);
#line 28 "E:\\codez\\src\\groovesizerstochastic\\midi.ino"
void midiSendStop(void);
#line 32 "E:\\codez\\src\\groovesizerstochastic\\midi.ino"
static void HandleNoteOn(byte channel, byte pitch, byte velocity);
#line 70 "E:\\codez\\src\\groovesizerstochastic\\midi.ino"
static void HandleNoteOff(byte channel, byte pitch, byte velocity);
#line 75 "E:\\codez\\src\\groovesizerstochastic\\midi.ino"
static void HandleClock(void);
#line 92 "E:\\codez\\src\\groovesizerstochastic\\midi.ino"
static void HandleStart(void);
#line 103 "E:\\codez\\src\\groovesizerstochastic\\midi.ino"
void HandleStop(void);
#line 111 "E:\\codez\\src\\groovesizerstochastic\\midi.ino"
void midiSetThru(void);
#line 120 "E:\\codez\\src\\groovesizerstochastic\\midi.ino"
void midiSetup(void);
#line 32 "E:\\codez\\src\\groovesizerstochastic\\pots.ino"
void potsUpdate();
#line 2 "E:\\codez\\src\\groovesizerstochastic\\sequencer.ino"
void seqSetup(void);
#line 59 "E:\\codez\\src\\groovesizerstochastic\\sequencer.ino"
void scheduleNext(void);
#line 85 "E:\\codez\\src\\groovesizerstochastic\\sequencer.ino"
void seqCheckState(void);
#line 107 "E:\\codez\\src\\groovesizerstochastic\\sequencer.ino"
void seqSetTransportState(uint8_t state);
#line 1 "E:\\codez\\src\\groovesizerstochastic\\setup.ino"
void setup();
#line 731 "E:\\codez\\src\\groovesizerstochastic\\ui.ino"
void uiAcceptInput(void);
#line 19 "E:\\codez\\src\\groovesizerstochastic\\buttons.ino"
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
  uint8_t clockDivider : 3; //0=default (1x) sets speed 0=1, 1=2, 2=4x or 3=1/2, 4=1/4, 5=1/8x, 6=3/4
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

#line 1 "E:\\codez\\src\\groovesizerstochastic\\buttons.ino"
/**********************************************************************************************
 ***
 *** Button checker with shift-in for 4 CD4021Bs parallel to serial shift registers (32 buttons)
 *** Adapted from Adafruit http://www.adafruit.com/blog/2009/10/20/example-code-for-multi-button-checker-with-debouncing/
 *** ShiftIn based on  http://www.arduino.cc/en/Tutorial/ShiftIn
 ***
 ***********************************************************************************************/
// min milliseconds that must elapse between button checks
#define DEBOUNCE_TIME 5
#define BUTTON_CLEAR_JUST_INDICATORS() (memset(gButtonJustChanged,0,BUTTON_BYTES))  

// setup code for reading the buttons via CD4021B parallel to serial shift registers
// based on ShiftIn http://www.arduino.cc/en/Tutorial/ShiftIn
const byte BUTTONlatchPin = 7;
const byte BUTTONclockPin = 8;
const byte BUTTONdataPin = 9;

void
buttonSetup()
{
  pinMode(BUTTONlatchPin, OUTPUT);
  pinMode(BUTTONclockPin, OUTPUT); 
  pinMode(BUTTONdataPin, INPUT);
}

//***SHIFTIN***********************************************************************************
// definition of the shiftIn function
// just needs the location of the data pin and the clock pin
// it returns a byte with each bit in the byte corresponding
// to a pin on the shift register. leftBit 7 = Pin 7 / Bit 0= Pin 0

static uint8_t 
shiftIn(int myDataPin, int myClockPin) { 
  uint8_t i;
  int pinState;
  uint8_t myDataIn = 0;

  pinMode(myClockPin, OUTPUT);
  pinMode(myDataPin, INPUT);

  //we will be holding the clock pin high 8 times (0,..,7) at the
  //end of each time through the for loop

  //at the begining of each loop when we set the clock low, it will
  //be doing the necessary low to high drop to cause the shift
  //register's DataPin to change state based on the value
  //of the next bit in its serial information flow.
  //The register transmits the information about the pins from pin 7 to pin 0
  //so that is why our function counts down
  for (i=0; i < 8; i++)
  {
    digitalWrite(myClockPin, LOW);
    delayMicroseconds(2);
    if (digitalRead(myDataPin))
      myDataIn |=  (1 << i);    
    digitalWrite(myClockPin, HIGH);

  }
  return myDataIn;
}

void buttonsUpdate()
{
  static uint8_t previousBtnState[BUTTON_BYTES]; // one bit for each
  static uint8_t currentBtnState[BUTTON_BYTES]; // one bit for each
  static uint8_t debounceLastTime;
  uint8_t index, mask, btnByte, currentState;
  

  // debounce. No need to take up more than a byte which gives us a range of 255 milliseconds 
  // we will definitely be in this function in much shorter periods than that.
  if((int8_t)((uint8_t)millis()-debounceLastTime) < DEBOUNCE_TIME)
    return; // not enough time has passed to debounce  
  debounceLastTime = (uint8_t)millis();
  

  //BUTTONS
  //while the shift register is in serial mode
  //collect each shift register into a byte
  //the register attached to the chip comes in first 
  for (index = 0; index < BUTTON_BYTES; index++)
    currentBtnState[index] = shiftIn(BUTTONdataPin, BUTTONclockPin);

  // when we start, we clear out the "just" indicators
  BUTTON_CLEAR_JUST_INDICATORS();
  
  
  for (index = 0; index < 40; index++) {
    btnByte=index/8;        // which byte to address in each array
    mask = 1 << (index%8);  // used to extract the bit we want
    currentState=currentBtnState[btnByte] & mask;
    if(currentState == (previousBtnState[btnByte] & mask)) { // debounce state stable
      // if it's not what it was, say that it's just changed
      if((gButtonIsPressed[btnByte] & mask) != currentState)
        gButtonJustChanged[btnByte] |= mask; // set the bit to 1

      // this allows us to check whether the button is down
      gButtonIsPressed[btnByte] &= (~mask); // turn the bit off      
      gButtonIsPressed[btnByte] |= currentState; // set to match current state
        
    } else {      
      // save the current state since it differs from previous
      previousBtnState[btnByte] &= (~mask); // turn bit off
      previousBtnState[btnByte] |= currentState; // set bit to match current state
    }
  }
}

 

#line 1 "E:\\codez\\src\\groovesizerstochastic\\clock.ino"
// this will (in addition to handling internal clock) be responsible
// for sending out a midi clock tick if enabled
static uint8_t stepCounter;

/* This gets called when timer1 ticks. This corresponds to BPM so it
 *  will get called 96 times per quarter note.
 *  It will not be called at all when we are in midi slave mode
 *  It's purpose is to just set lastStepTime
 */
void internalClock()
{
  /* Midi clock is sent 24 times per quarter note.
   * Since we get called 96 times per qn, only send every 4th time
  */
  if((stepCounter % 4==0) &&
    gSeqState.midiSendClock && 
    gRunningState.transportState==TRANSPORT_STARTED) {    
    
    midiSendClock();
  }
    
  // step occurs every 24 ticks (96/4 as there are 4 steps per beat)
  if((stepCounter%24)==0){
    gRunningState.lastStepTime = millis();
  }

  stepCounter++;
  if(stepCounter > 23)
    stepCounter=0;  
}

// ensure that next clock tick starts at step boundary
// (user just hit start)
void clockSetStarted(void)
{
  // this means that when you hit start, you could have a miniscule delay until next timer click
  stepCounter=0;  
  
}

// things that need to happen as a consequence of the bpm changing
void clockSetBPM(uint8_t BPM)
{
  Timer1.detachInterrupt(); // remove callback fn if any
  gSeqState.tempo=BPM;
  if(BPM==0) { // bpm of 0 indicates ext clock sync    
    gRunningState.tempo=DEFAULT_BPM; // until we get clock sync we can't know what the tempo is        
    // TODO start receiving midi clock
  } else {
    gRunningState.tempo=BPM;
    // use 24 ticks per beat - same as midi clock
    // TODO check for big rounding, or intermediate overflow
    Timer1.setPeriod(1000000/(BPM*24/60)); // with period in microseconds (same as 1000000*60/BPM/24)
    Timer1.attachInterrupt(internalClock);
  }
}

void
clockSetup(void)
{
  // setup for the sequencer timer (default timer interval is 1000ms)
  Timer1.initialize(0);
  // initial clock setup
  clockSetBPM(gSeqState.tempo); 
}



#line 1 "E:\\codez\\src\\groovesizerstochastic\\eeprom.ino"
EEPROM256_512 mem_1;            // define the eeprom chip
uint8_t rwBuffer[64];           // our EEPROM read/write buffer

void
eepromSetup(void)
{
    //begin I2C Bus
  Wire.begin();
  //begin EEPROM with I2C Address 
  mem_1.begin(0,0);//addr 0 (DEC) type 0 (defined as 24LC256)
}
#if 0 
// for example

  // check if character definitions exists in EEPROM by seeing if the first 10 bytes of the expected character array add up to 93
  mem_1.readPage(508, rwBuffer); // 0 is the first page of the EEPROM (511 is last) - a page is 64 bytes long
  byte x = 0;
  for (byte i = 0; i < 10; i++)
  {
    x = x + rwBuffer[i];
  }
  if (x != 93) // yikes, there's a problem - the EEPROM isn't set up properly
  {
    //give us a bad sign!
    LEDrow[0] =  B10101010;
    LEDrow[1] =  B01010101;
    LEDrow[2] =  B10101010;
    LEDrow[3] =  B01010101;
    LEDrow[4] =  B10101010;
    for (byte i = 0; i < 100; i++)
    { 
      updateLeds();
      delay(5);
    }  
  }

  else
  {
    //give us a good sign!
    for (byte i = 0; i < 5; i++)
      LEDrow[i] =  B11111111;

    for (byte i = 0; i < 100; i++)
    { 
      updateLeds();
      delay(5);
    }
  }
// read the table of contents (TOC) from the EEPROM (page 448)
// also recalls preferences
void tocRead()
{
  mem_1.readPage(448, rwBuffer);
  for (byte i = 0; i < 14; i++)
  {
    toc[i] = rwBuffer[i];
  }

  // update the preferences
  midiSyncOut = rwBuffer[14]; // send out 24ppq clock pulses to slave MIDI devices to this Groovesizer
  thruOn = rwBuffer[15]; // echo MIDI data received at input at output
  checkThru();
  midiTrigger = rwBuffer[16]; // send pattern trigger notes to change patterns on slaved Groovesizers
  triggerChannel = rwBuffer[17]; // the MIDI channel that pattern change messages are sent and received on
}

void tocWrite(byte addLoc)
{
  byte tocByte = addLoc/8; // in which byte of the toc array is the location (each byte corresponds to a row of buttons)
  toc[tocByte] = bitSet(toc[tocByte], addLoc%8); // turns the appropriate byte on (1)
  for (byte i = 0; i < 14; i++) // update the read/write buffer with the current toc
    rwBuffer[i] = toc[i];

  // remember to save preferences too, since they're on the same EEPROM page
  assignPreferences();

  mem_1.writePage(448, rwBuffer); // write the buffer to the toc page on the EEPROM (448)
}

void tocClear(byte clearLoc)
{
  byte tocByte = clearLoc/8; // in which byte of the toc array is the location (each byte corresponds to a row of buttons)
  toc[tocByte] = bitClear(toc[tocByte], clearLoc%8); // turns the appropriate byte off (0)
  for (byte i = 0; i < 14; i++) // update the read/write buffer with the current toc
    rwBuffer[i] = toc[i];

  // remember to save preferences too, since they're on the same EEPROM page
  assignPreferences();

  mem_1.writePage(448, rwBuffer); // write it to the toc page on the EEPROM (448)

}

boolean checkToc(byte thisLoc) // check whether a memory location is already used or not
{
  byte tocByte = thisLoc/8; // in which byte of the toc array is the location (each byte corresponds to a row of buttons)
  if bitRead(toc[tocByte], thisLoc%8)
    return true;
  else
    return false;  
}

void savePrefs()
{
  //preferences are saved on the same EEPROM page (448) as the toc, so make sure to resave the toc when saving preferences
  for (byte i = 0; i < 14; i++) // update the read/write buffer with the current toc
    rwBuffer[i] = toc[i];

  assignPreferences(); // defined above

  mem_1.writePage(448, rwBuffer); // write the buffer to the toc page on the EEPROM (448) 
}

void savePatch()
{
  //transposeFactor = 0; // reset the transpose factor to 0
  static boolean pageSaved = false;
  static byte progress; // we want to stagger the save process over consecutive loops, in case the process takes too long and causes an audible delay
  if (!pageSaved)
  {    
    BUTTON_CLEAR_JUST_INDICATORS();
    longPress = 0;
    //mem_1.readPage(pageNum, rwBuffer);
    pageSaved = true;
    progress = 0;
  }

  switch (progress) { // we want to break our save into 4 seperate writes on 4 consecutive passes through the loop
  case 0:
    // pack our 64 byte buffer with the goodies we want to send to the EEPROM

    for (byte i = 0; i < 3; i++) // for the first set of 3 tracks
    {
      packTrackBuffer(i);
    }  

    mem_1.writePage(pageNum, rwBuffer);
    progress = 1;
    break;

  case 1:

    for (byte i = 3; i < 6; i++) // for the second set of 3 tracks
    {
      packTrackBuffer(i);
    }  

    // add the division bytes
    rwBuffer[57] = clockDivSelect;
    rwBuffer[58] = clockDivSlaveSelect;

    mem_1.writePage(pageNum + 1, rwBuffer);
    progress = 2;
    break;

  case 2:
    for (byte i = 6; i < 9; i++) // for the third set of 3 tracks
    {
      packTrackBuffer(i);
    }  

    // add first 7 of 14 master page bytes
    rwBuffer[57] = seqLength;
    rwBuffer[58] = seqFirstStep;
    rwBuffer[59] = programChange;
    for (byte i = 0; i < 4; i++)
      rwBuffer[60 + i] = seqStepMute[i];

    mem_1.writePage(pageNum + 2, rwBuffer);
    progress = 3;
    break;

  case 3:
    for (byte i = 9; i < 12; i++) // for the fourth set of 3 tracks
    {
      packTrackBuffer(i);
    }

    // add second 7 of 14 master page bytes
    rwBuffer[57] = swing;
    rwBuffer[58] = followAction;
    rwBuffer[59] = bpm;
    for (byte i = 0; i < 4; i++)
      rwBuffer[60 + i] = seqStepSkip[i];  

    mem_1.writePage(pageNum + 3, rwBuffer);
    save = false;
    pageSaved = false;
    break;
  }
}

void packTrackBuffer(byte Track)
{
  byte offset = 19 * (Track % 3); // each track is 19 bytes long
  rwBuffer[0 + offset] = track[Track].level;       
  rwBuffer[1 + offset] = track[Track].accentLevel;
  rwBuffer[2 + offset] = highByte(track[Track].flamDelay); // flamDelay stored as 2 bytes - the int is split into a high and low byte 
  rwBuffer[3 + offset] = lowByte(track[Track].flamDelay);
  rwBuffer[4 + offset] = track[Track].flamDecay;
  rwBuffer[5 + offset] = track[Track].midiChannel;
  rwBuffer[6 + offset] = track[Track].midiNoteNumber;
  for (byte j = 0; j < 4; j++)
  {
    rwBuffer[7 + offset + j] = track[Track].stepOn[j];
    rwBuffer[11 + offset + j] = track[Track].stepAccent[j];
    rwBuffer[15 + offset + j] = track[Track].stepFlam[j];
  } 
}

void unPackTrackBuffer(byte Track)
{
  byte offset = 19 * (Track % 3); // each track is 19 bytes long
  track[Track].level = rwBuffer[0 + offset];       
  track[Track].accentLevel = rwBuffer[1 + offset];
  track[Track].flamDelay = word(rwBuffer[2 + offset], rwBuffer[3 + offset]);
  track[Track].flamDecay = rwBuffer[4 + offset];
  track[Track].midiChannel = rwBuffer[5 + offset];
  track[Track].midiNoteNumber = rwBuffer[6 + offset];
  for (byte j = 0; j < 4; j++)
  {
    track[Track].stepOn[j] = rwBuffer[7 + offset + j];
    track[Track].stepAccent[j] = rwBuffer[11 + offset + j];
    track[Track].stepFlam[j] = rwBuffer[15 + offset + j];
  } 
}

void loadPatch(byte patchNumber) // load the specified location number
{
  saved = true; // needed for follow actions 1 and 2
  recall = false;
  pageNum = patchNumber * 4; // each location is 4 pages long
  clearSelected();
  BUTTON_CLEAR_JUST_INDICATORS();
  cued = patchNumber; // for the duration of the load process, we set cued = patchNumber so we can loadContinue with loadPatch(cued)

  mem_1.readPage(pageNum, rwBuffer);

  // unpack our 64 byte buffer with the goodies from the EEPROM

  for (byte i = 0; i < 3; i++) // for the first set of 3 tracks
  {
    unPackTrackBuffer(i);
  }  

  mem_1.readPage(pageNum + 1, rwBuffer);

  for (byte i = 3; i < 6; i++) // for the second set of 3 tracks
  {
    unPackTrackBuffer(i);
  }  

  // unpack the division bytes
  clockDivSelect = rwBuffer[57];
  clockDivSlaveSelect = rwBuffer[58];

  mem_1.readPage(pageNum + 2, rwBuffer);

  for (byte i = 6; i < 9; i++) // for the third set of 3 tracks
  {
    unPackTrackBuffer(i);
  }  

  // read first 7 of 14 master page bytes
  seqLength = rwBuffer[57];
  seqFirstStep = rwBuffer[58];
  seqLastStep = seqFirstStep + (seqLength - 1);
  // send a program change message if the patch is not the same as the current one
  if (programChange != rwBuffer[59])
  {
    programChange = rwBuffer[59];
    midiA.sendProgramChange(programChange, track[0].midiChannel);
  }
  for (byte i = 0; i < 4; i++)
    seqStepMute[i] = rwBuffer[60 + i];

  mem_1.readPage(pageNum + 3, rwBuffer);

  for (byte i = 9; i < 12; i++) // for the fourth set of 3 tracks
  {
    unPackTrackBuffer(i);
  }

  // read second 7 of 14 master page bytes
  swing = rwBuffer[57];
  followAction = rwBuffer[58];
  bpm = rwBuffer[59];
  bpmChange(bpm);
  for (byte i = 0; i < 4; i++)
    seqStepSkip[i] = rwBuffer[60 + i];  

  nowPlaying = cued; // the pattern playing is the one that was cued before
  cued = 255; // out of range since 112 patterns only - we'll use 255 to see if something is cue
}

#endif

#line 1 "E:\\codez\\src\\groovesizerstochastic\\helper.ino"
// helpers







#line 1 "E:\\codez\\src\\groovesizerstochastic\\leds.ino"
// for the rows of LEDs
// the byte that will be shifted out 

// time in ms to display temporary values (like if a pot is twiddled)
#define LED_TEMP_DISPLAY_TIME 1000

#define LED_BYTES 5
byte LEDrow[LED_BYTES];

// will be set to true if we are currently showing a temporary value on the display
uint8_t LEDShowingTemporary;
uint16_t LEDTimer; // timer for temporary value

// setup code for controlling the LEDs via 74HC595 serial to parallel shift registers
// based on ShiftOut http://arduino.cc/en/Tutorial/ShiftOut
const byte LEDlatchPin = 2;
const byte LEDclockPin = 6;
const byte LEDdataPin = 4;

void 
ledsSetup(void)
{
  //define pin modes 
  pinMode(LEDlatchPin, OUTPUT);
  pinMode(LEDclockPin, OUTPUT); 
  pinMode(LEDdataPin, OUTPUT);
  LEDTimer=0;
  LEDShowingTemporary=0;
}

// this is used when we are not displaying a temporary value.
// it does a full update based on the current state of things
static void ledsFullUpdate()
{
#if 0  
  // blink the led for each step in the sequence
  static byte stepLEDrow[4];
  // controlLEDrow is defined as global variable above so we can set it from helper functions
  static unsigned long ledBlink; // for blinking an LED
  static unsigned long ledBlink2; // for blinking another LED 
  // ******************************************
  //     LED setup: work out what is lit
  //     end of button check if/else structure
  // ******************************************
  if (showingNumbers)
  {
    showNumber();
  }
  else if (showingWindow)
  {
    for (byte i = seqFirstStep; i < (seqFirstStep + seqLength); i++)
      bitSet(LEDrow[i / 8], i % 8);
  }
  else
  {
    if (mode < 3) // editing tracks 0 - 11
    {
      // we always start with steps that are On and then work out what to blink or flash from there
      for (byte i = 0; i < 32; i++)
      {
        if (checkStepOn(currentTrack, i))
          bitSet(LEDrow[i / 8], i % 8); 
      }
    }

    if (mode == 3)
    {
      for (byte i = 0; i < 12; i++) // show all the active steps (on all tracks) on one grid
      {
        for (byte j = 0; j < 4; j++)
          LEDrow[j] |= track[i].stepOn[j]; 
      }
    }

    if (mode < 4 && mode > 0) // blink selected steps if we're not in trigger mode
    {
      // blink step selected leds
      if (millis() > ledBlink + 100)
      {
        for (byte i = 0; i < 4; i++)
          LEDrow[i] ^= *edit[i];

        if (millis() > ledBlink + 200)
          ledBlink = millis(); 
      } 
    }

    if (mode == 4) // we're in trigger mode
    {
      for (byte i = 0; i < 4; i++)
        LEDrow[i] = toc[trigPage*4 + i];

      // slow blink nowPlaying
      if (nowPlaying < 255 && nowPlaying / 32 == trigPage) // we use 255 to turn nowPlaying "off", and only show if we're on the appropriate trigger page
      {
        if (millis() > ledBlink + 200)
        {
          LEDrow[(nowPlaying % 32) / 8] ^= 1<<(nowPlaying%8);

          if (millis() > ledBlink + 400)
            ledBlink = millis(); 
        }
      } 

      // (fast) blink the pattern that's cued to play next
      if (cued < 255 && cued / 32 == trigPage) // we use 255 turn cued "off", and only show if we're on the appropriate trigger page
      {
        if (millis() > ledBlink2 + 100)
        {
          LEDrow[(cued % 32) / 8] ^= 1<<(cued%8);

          if (millis() > ledBlink2 + 200)
            ledBlink2 = millis(); 
        }
      }

      // (fast) blink the location that needs to be confirmed to overwrite
      else if (confirm < 255 && confirm / 32 == trigPage)
      {
        if (millis() > ledBlink2 + 100)
        {
          LEDrow[(confirm % 32) / 8] ^= 1<<(confirm%8);

          if (millis() > ledBlink2 + 200)
            ledBlink2 = millis(); 
        }
      }

      bitClear(controlLEDrow, 6);
      bitClear(controlLEDrow, 5);
    }

    // this blinks the led for the current step
    if (seqRunning || syncStarted)
    {
      for (byte i = 0; i < 4; i++)
      {
        stepLEDrow[i] = (lastStep / 8 == i) ? B00000001 << (lastStep % 8) : B00000000;
        LEDrow[i] ^= stepLEDrow[i];
      }
    }

    // update the controlLEDrow
    switch (mode)
    {
    case 0:
      controlLEDrow = B00000010;
      controlLEDrow = controlLEDrow << currentTrack % 6;
      if (currentTrack > 5)
        bitSet(controlLEDrow, 7);
      break;
    case 3:
      controlLEDrow = B10000001; // master page
      if (seqRunning || syncStarted)
        bitSet(controlLEDrow, 1);
      if (mutePage)
        bitSet(controlLEDrow, 3);
      if (skipPage)
        bitSet(controlLEDrow, 4);        
      break;
    case 4:
      controlLEDrow = B00000001; // trigger page
      controlLEDrow = bitSet(controlLEDrow, trigPage + 1); // light the LED for the page we're on
      if (followAction == 1 || followAction == 2)  // light the LED for followAction 1 or 2
          controlLEDrow = bitSet(controlLEDrow, 7 - followAction);
      break;
    case 5:
      controlLEDrow = 0; // clear it
      // set according to the preferences
      if (midiSyncOut)
        bitSet(controlLEDrow, 1);
      if (thruOn)
        bitSet(controlLEDrow, 2);
      if (midiTrigger)
        bitSet(controlLEDrow, 3);
      break;
    }

    LEDrow[4] = controlLEDrow; // light the LED for the mode we're in
  }
#endif  
}

/**********************************************************************************************
 ***
 *** ShiftOut to address 42 LEDs via 4 74HC595s serial to parallel shifting registers 
 *** Based on http://arduino.cc/en/Tutorial/ShiftOut
 ***
 ***********************************************************************************************/
static void shiftOut(int myLEDdataPin, int myLEDclockPin, byte myDataOut) {
  // This shifts 8 bits out MSB first, 
  // on the rising edge of the clock,
  // clock idles low

  //internal function setup
  int i=0;
  int pinState;
  pinMode(myLEDclockPin, OUTPUT);
  pinMode(myLEDdataPin, OUTPUT);

  //clear everything out just in case to
  //prepare shift register for bit shifting
  digitalWrite(myLEDdataPin, 0);
  digitalWrite(myLEDclockPin, 0);

  //for each bit in the byte myDataOut
  //NOTICE THAT WE ARE COUNTING DOWN in our for loop
  //This means that %00000001 or "1" will go through such
  //that it will be pin Q0 that lights. 
  for (i=7; i>=0; i--)  {
    digitalWrite(myLEDclockPin, 0);

    //if the value passed to myDataOut and a bitmask result 
    // true then... so if we are at i=6 and our value is
    // %11010100 it would the code compares it to %01000000 
    // and proceeds to set pinState to 1.
    if ( myDataOut & (1<<i) ) {
      pinState= 1;
    }
    else {  
      pinState= 0;
    }

    //Sets the pin to HIGH or LOW depending on pinState
    digitalWrite(myLEDdataPin, pinState);
    //register shifts bits on upstroke of clock pin  
    digitalWrite(myLEDclockPin, 1);
    //zero the data pin after shift to prevent bleed through
    digitalWrite(myLEDdataPin, 0);
  }

  //stop shifting
  digitalWrite(myLEDclockPin, 0);
}


// turn off all leds
static void ledsOff(void)
{
  memset(LEDrow,0,LED_BYTES);
  
}

// @AS this makes no sense to me... needs to be called twice to
// see the screen and then the led's are on 50% duty cycle. is it really necessary? 
// this function shifts out the the 4 bytes corresponding to the led rows
static void ledsLightUp(void)
{
  static boolean lastSentTop = false; 
  // we want to alternate sending the top 2 and bottom 3 rows to prevent an edge 
  // case where 4 rows of LEDs lit at the same time sourcing too much current
  
  // ground LEDlatchPin and hold low for as long as you are transmitting
  digitalWrite(LEDlatchPin, 0);
  if (!lastSentTop) // send the top to rows
  {
    shiftOut(LEDdataPin, LEDclockPin, B00000000);
    shiftOut(LEDdataPin, LEDclockPin, B00000000); 
    shiftOut(LEDdataPin, LEDclockPin, B00000000);
    shiftOut(LEDdataPin, LEDclockPin, LEDrow[1]); 
    shiftOut(LEDdataPin, LEDclockPin, LEDrow[0]);
    lastSentTop = true;
  }
  else // ie. lastSentTop is true, then send the bottom 3 rows
  {
    shiftOut(LEDdataPin, LEDclockPin, LEDrow[4]);
    shiftOut(LEDdataPin, LEDclockPin, LEDrow[3]); 
    shiftOut(LEDdataPin, LEDclockPin, LEDrow[2]);
    shiftOut(LEDdataPin, LEDclockPin, B00000000); 
    shiftOut(LEDdataPin, LEDclockPin, B00000000);
    lastSentTop = false;
  }
  //return the latch pin high to signal chip that it 
  //no longer needs to listen for information
  digitalWrite(LEDlatchPin, 1);
}

// TODO move this to progmem? performance might be slower
void ledsShowNumber(uint8_t number)
{
 static byte nums[25] = 
  {
    B01110010,
    B01010010,
    B01010010,
    B01010010,
    B01110010,

    B01110111,
    B00010001,
    B01110111,
    B01000001,
    B01110111,

    B01010111,
    B01010100,
    B01110111,
    B00010001,
    B00010111,

    B01000111,
    B01000001,
    B01110001,
    B01010001,
    B01110001,

    B01110111,
    B01010101,
    B01110111,
    B01010001,
    B01110001    
  };

  byte offset = 0; // horizontal offset for even numbers 
  byte row = 0; // vertical offset for the start of the digit

  ledsOff();

  if (number > 199) // add two dots for numbers 200 and over
  {
    LEDrow[3] = B00000001;
    LEDrow[4] = B00000001;
  }

  else if (number > 99) // add a dot for numbers 100 and over
    LEDrow[4] = B00000001;

  //digits under 10
  if ((number % 10) % 2 == 0) // even numbers
    offset = 4;
  else
    offset = 0;
  row = ((number % 10)/2)*5;      

  for (byte j = 0; j < 5; j++)
  {
    for (byte i = 0; i < 3; i++) // 3 pixels across
    {      
      if (bitRead(nums[row + j], offset + i)) // ie. if it returns 1  
        LEDrow[j] = bitSet(LEDrow[j], 7 - i);
    }
  }

  // digits over 10
  if (((number / 10) % 10) % 2 == 0) // even numbers
    offset = 4;
  else
    offset = 0;
  row = (((number / 10) % 10)/2)*5;      

  for (byte j = 0; j < 5; j++)
  {
    for (byte i = 0; i < 3; i++) // 3 pixels across
    {      
      if (bitRead(nums[row + j], offset + i)) // ie. if it returns 1  
        LEDrow[j] = bitSet(LEDrow[j], 3 - i);
    }
  }

  // start the countdown for showing the number
  LEDShowingTemporary=1;
  LEDTimer=millis();
}

// called from main loop
void ledsUpdate(void)
{
  // if we are displaying a temporary value, just let it abide
  if(LEDShowingTemporary) {
    // if enough time has passed...
    if((int16_t)((uint16_t)millis()-LEDTimer) > LED_TEMP_DISPLAY_TIME) {
      // stop showing it
      LEDShowingTemporary=0;
    } else
      return;
  }

  // full update based on current state
  ledsFullUpdate();
  
  // light the actual lights
  ledsLightUp();
}


#line 1 "E:\\codez\\src\\groovesizerstochastic\\mainloop.ino"
// Main loop
void loop() 
{
  
  // This will invoke callbacks - see HandleMidi   
  midiUpdate();
  
  
  // see if any buttons are pressed. If they were just pressed, the just indicators will be set
  // and can be checked below this line (they are reset every time this is called)
  buttonsUpdate();

  // get the pot values (also updates just-twiddled)
  potsUpdate();

  // respond to user. 
  uiAcceptInput();
  
  // do sequencer - advance tracks, patterns, etc.
  // this may trigger midi notes
  seqCheckState();
  
  // update screen to reflect reality
  ledsUpdate();
  
}






#line 1 "E:\\codez\\src\\groovesizerstochastic\\midi.ino"
// midiA.h library info http://playground.arduino.cc/Main/MIDILibrary
// Reference http://arduinomidilib.fortyseveneffects.com/

// creates a global var that represents midi
// (size of midi is 183)
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, midiA);

// used to keep track of when we reach step boundaries
static uint8_t midiTicks;

// called from the main loop.
// read midi port, issue callbacks
void midiUpdate(void)
{
  midiA.read();
}

void midiSendClock(void)
{
  midiA.sendRealTime(midi::Clock);
}

void midiSendStart(void)
{
  midiA.sendRealTime(midi::Start);
}

void midiSendStop(void)
{
  midiA.sendRealTime(midi::Stop); // send a midi clock stop signal)  
}
static void HandleNoteOn(byte channel, byte pitch, byte velocity) 
{ 
  #if 0
  // Do whatever you want when you receive a Note On.
  // Try to keep your callbacks short (no delays ect) as the contrary would slow down the loop()
  // and have a bad impact on real-time performance.
  if (channel == 10 && pitch < 112) // receive patch changes on triggerChannel (default channel 10); there are 112 memory locations
  {      
    if (checkToc(pitch))
    {
      recall = true; // we're ready to recall a preset
      pageNum = pitch * 4; // one memory location is 4 pages long
      cued = pitch; // the pattern that will play next is the value of "pitch" - need this to blink lights
      head = pitch; // need this for pattern chaining to work properly
      mode = 4; // make sure we're in trigger mode, if we weren't already
      trigPage = pitch/32; // switch to the correct trigger page
      controlLEDrow = B00000001; // light the LED for trigger mode
      controlLEDrow = bitSet(controlLEDrow, trigPage + 1); // light the LED for the page we're on     
    }
  }
  else if (mode != 7  && pitch > 0 && channel == 10)
  {
    if (!seqRunning && !midiClock)
    { 
      if (velocity != 0) // some instruments send note off as note on with 0 velocity
      {
        lastNote = pitch;
        int newVelocity = (pot[2] + velocity < 1023) ? pot[2] + velocity : 1023; 
      }
      else // velocity = 0;
      {
      }
    }
  }
  #endif
}


static void HandleNoteOff(byte channel, byte pitch, byte velocity) 
{
} 

// Called 24 times per QN when we are receiving midi clock
static void HandleClock(void)
{  
  if(SLAVE_MODE()) { // slave mode
    // 6 ticks per step interval
    if(midiTicks % 6==0) {
      gRunningState.lastStepTime=millis();
    }
  }
  
  midiTicks++;
  if(midiTicks > 23)
    midiTicks=0;
}

/* Called when we receive a MIDI start
 *  
 */
static void HandleStart (void)
{
  midiTicks=24; // 0th tick. handleclock will be called next with first timing clock
  if(SLAVE_MODE()) { // slave mode
    seqSetTransportState(TRANSPORT_STARTED);
  }
}

/* Called when we receive a midi stop
 *  
 */
void HandleStop (void)
{
  if(SLAVE_MODE()) { // slave mode
    seqSetTransportState(TRANSPORT_STOPPED);
  }  
}

// TODO where do we call this from?
void midiSetThru(void)
{
  if(gSeqState.midiThru) {
    midiA.turnThruOn(midi::Full);
  } else {
    midiA.turnThruOff();
  }  
}

void midiSetup(void)
{
  // Initiate MIDI communications, listen to all channels
  midiA.begin(MIDI_CHANNEL_OMNI);    

  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  midiA.setHandleNoteOn(HandleNoteOn);  // only put the name of the function here - functions defined in HandleMidi
  midiA.setHandleNoteOff(HandleNoteOff);
  midiA.setHandleClock(HandleClock);
  midiA.setHandleStart(HandleStart);
  midiA.setHandleStop(HandleStop); 

  // setup based on current sequencer state
  midiSetThru();
}


#line 1 "E:\\codez\\src\\groovesizerstochastic\\pots.ino"
#define POTS_AVG_COUNT 4
#define HYSTERESIS     3
#define ANALOGPIN      0      // the analog pin connected to multiplexer output
/* @AS read the pot values and save a bit as to whether they've changed
 */ 
void
potsSetup(void)
{
  // set the three 4051 select pins to output
  // the pins are wired to the multiplexer select bit and are used to select 1 of 8 inputs on multiplexer
  pinMode(17,OUTPUT);
  pinMode(16,OUTPUT);
  pinMode(15,OUTPUT);  
}

// POTS reading from 4051
// this function returns the analog value for the given channel
static uint16_t
getPotValue(uint8_t channel)
{
  // set the selector pins HIGH and LOW to match the binary value of channel
  digitalWrite(17, bitRead(channel, 0));
  digitalWrite(16, bitRead(channel, 1));
  digitalWrite(15, bitRead(channel, 2));  
  // we're only using readings between 25 and 1000, as the extreme ends are unstable
  return map((constrain(analogRead(ANALOGPIN), 25, 1000)), 25, 1000, 0, 1023); 
  
}

// determine the current value of the pots
// (called from main loop)
void potsUpdate()
{
  uint8_t ctr;
  uint16_t val, sum;
  gPotChangeFlag=0;
  for(uint8_t i=0;i<POT_COUNT;i++) {// each pot
    // get an average of 4 readings
    for(uint8_t ctr=0, sum=0; ctr<POTS_AVG_COUNT; ctr++)
      sum+=getPotValue(i);
    val=sum/POTS_AVG_COUNT; 
    // don't want underflows here when comparing
    // HYSTERESIS value was taken from LXR code where it seems fine. Original code here had 35 as the threshold for
    // indicating a changed value
    if ((val > ( gPotValue[i] + HYSTERESIS)) ||
      (gPotValue[i] > (val + HYSTERESIS)))
      gPotChangeFlag |= (1 << i); // indicate that it's value has changed                     
  }  
}



#line 1 "E:\\codez\\src\\groovesizerstochastic\\sequencer.ino"
// set initial sequencer and running state
void seqSetup(void)
{
  SeqStep *step;
  SeqTrack *track;
  // initial 0 state should be fine for most things
  gSeqState.randomRegen = 1;
  gSeqState.tempo=DEFAULT_BPM;

  for(uint8_t i=0;i<NUM_PATTERNS;i++) {
    for(uint8_t j=0;j<NUM_TRACKS;j++) {
      track=&gSeqState.patterns[i].tracks[j];
      track->numSteps=NUM_STEPS-1; // 0 based
      track->midiNote=DEFAULT_MIDI_NOTE;
    }    
  }
}

/*
 * scheduleNext: This is called from main loop. lastStepTime will be set to the last time that
 * a 4ppq clock tick occurred (ie a step). Under normal circumstances, we will schedule the next play time
 * of the track for that time. However we might have a clock divider or a swing to take into account.
 * Clock divider:
 *   4x or 2x:
 *     These need to occur 4 times (or 2) for each step, so we will use divPosition to determine where we are
 *     example (divider is 4x):
 *     isScheduled is false, lastStepTimeTriggered is true, we set it to false in this fn
 *     lastStepTime is set to 10 track position is 0 (no steps have played). divPosition is 0
 *     we set scheduledStart to 10, increment divPosition to 1 and set isScheduled to true
 *     (another function picks up the scheduled time, plays the note and clears isScheduled, sets trackposition to 1)
 *     lastStepTime is still 10, track position is 1 (1 step has played). divPosition is 1
 *     we set scheduledStart to 10+((divPosition) * (length of step/divider) ), increment divPosition to 2 (divider is 4)
 *     when divPosition would overflow to 0 we don't want to schedule anything else until we get a new scheduledStart time divComplete is set true
 *     lastStepTime is 10, isSched is false, lsttrig is false, if divComplete is true, do nothing
 *     lastStepTime is 20 (next step), track position should be 4, divPosition should be 0 (but might be other if tempo sped up)
 *     if divposition is nonzero, increment position by (4-divposition) and set it to 0
 *     set schedStart to 20, increment divPosition to 1 and isSChed to true
 *     if divComplete flag was set, clear it
 *     
 *     when lastStepTimeTriggered is true, we need to assume that we are on a whole step boundary. This becomes important
 *     if the tempo is changing in real time. For example, if tempo was 80, clock div is 4, and tempo changes to 160 and we've
 *     played two steps, we don't want to be two steps off from the real beat (hard to explain, draw it)
 *     - if divPosition is non-zero we will skip 4-divPosition steps to ensure that steps line up properly
 *     
 *     example (divider is 1/4x) 4x slower than tempo:
 *     isScheduled is false, lastStepTrig is true, we set it to false
 *     lastSteptime is 10 position 0, divPosition 0
 *     set schedstart to 10, increment divPosition to 1 and set isSched to true
 *     (another fn plays it, sets tp to 1...)
 *     another call, lastStepTimeTriggered is false, isSched is now false, we see that divPosition is non-zero so we do nothing 
 *     another call, lastStepTimeTriggered is true, but since divPosition is non-zero, we just increment it and do nothing else 
 *     another call, lastStepTimeTriggered is true, divPosition is zero, we schedule the next step
 *     
 *     example divider is 3/4x:
 *     same as above but...
 *     
 *     
 */
void scheduleNext(void)
{
  // swing is only applied when clock div <= 1
  for(uint8_t i=0;i<NUM_TRACKS;i++) {  
    SeqTrack *track=&gSeqState.patterns[gRunningState.currentPattern].tracks[i];  
    TrackRunningState *trState=&gRunningState.trackPositions[i];

  gSeqState.swing;
    gSeqState.tempo; // if clock div, use to calculate position in time
  gRunningState.lastStepTime; // when we received the last step start tick
  track->clockDivider; //0=default (1x) sets speed 0=1,1=2,2=4x or 3=1/2, 4=1/4, 5=1/8x
  track->numSteps;
  track->muted;
  trState->position; // which step will be next (5 bits)
  trState->divPosition; // may be 0-3 if divx4 or 0-1 if divx2
  trState->isScheduled;  // whether or not we have scheduled the next note (reset to 0 when it's been played)
  trState->nextScheduledStart;// when it's scheduled to play (need to set this)
  
  
  
  }
  

}

// called from main loop to see if we need to alter sequencer state
void seqCheckState(void)
{
  uint8_t maxTrackLen=0;
  SeqPattern *curPattern=&gSeqState.patterns[gRunningState.currentPattern];  
  
  // determine biggest track for pattern,
  // also determine if we've reached the end of the pattern
  for(uint8_t i=0;i<NUM_TRACKS;i++) {
    if(curPattern->tracks[i].numSteps > maxTrackLen)
      maxTrackLen=curPattern->tracks[i].numSteps;
    //if(gRunningState.trackPositions[i].position==curPattern->tracks[i].numSteps)
  }
  maxTrackLen++; // remember - 0 based

  // check to see if we have reached the end of the pattern
  // if so, switch or not

  // check to see if any steps on any tracks need to play
  
  
}

void seqSetTransportState(uint8_t state)
{
  
  if(gRunningState.transportState==TRANSPORT_STOPPED) {
    // reset all track positions to 0
    // TODO 
    
  }

  // if we are sending midi clock, send start/stop
  if(gSeqState.midiSendClock) {
    switch(state) {
    case TRANSPORT_STARTED:
      midiSendStart();
      break;
    case TRANSPORT_STOPPED:
    case TRANSPORT_PAUSED:
      midiSendStop();
      break;        
    }
  }
  
  // finally update the state
  gRunningState.transportState=state;
}


// example
#if 0
void playStep()
{
  if (seqCurrentStep == 0 || seqCurrentStep == 16 || seqCurrentStep == seqFirstStep)
  {
    if (recall)
    {
      goLoad = true; // set a flag that we can now load a patch
    }
    //deal with the follow actions if we're on the first step
    else if (seqCurrentStep == 0 && followAction != 0)
    {
      switch (followAction)
      {
      case 1: // play the next pattern      
        if (checkToc(nowPlaying + 1) && saved == true) // check if there is a pattern stored in the next location
        {
          cued = nowPlaying + 1; // cue the next patch
          goLoad = true;
        }
        break;
      case 2:
        if (saved == true) // we don't want to go over page breaks (30 is the second to last location on the page), and check if there is a pattern stored in the next location
        {
          cued = head; // cue the patch marked as the head
          goLoad = true;
        }
        break;
      }
    }
  }

  if (!checkMute(seqCurrentStep)) // don't play anything if the step is muted on the master page
  {
    lastStep = seqCurrentStep; // used to blink the LED for the current step
    for (byte i = 0; i < 12; i++) // for each of the tracks
    {
      if (fxFlam[i] == 1 && (seqCurrentStep % 2 == 0) && !checkStepFlam(i, seqCurrentStep))
      {
        if (!syncStarted)
          scheduleFlam(i, track[i].accentLevel);
        else
          scheduleFlamSlave(i, track[i].accentLevel);
      }
      if (checkStepAccent(i, seqCurrentStep))
      {
        midiA.sendNoteOff(track[i].midiNoteNumber, track[i].accentLevel, track[i].midiChannel);
        midiA.sendNoteOn(track[i].midiNoteNumber, track[i].accentLevel, track[i].midiChannel);
        if (checkStepFlam(i, seqCurrentStep)) // schedule the first flam if this step is marked as flam
        {
          if (!syncStarted)
            scheduleFlam(i, track[i].accentLevel);
          else
            scheduleFlamSlave(i, track[i].accentLevel);
        }
        else // if the this step is not marked as a flam, turn off any sceduled flams
        track[i].nextFlamPulse = 0;
      }
      else if (checkStepOn(i, seqCurrentStep))
      {
        midiA.sendNoteOff(track[i].midiNoteNumber, track[i].level, track[i].midiChannel);
        midiA.sendNoteOn(track[i].midiNoteNumber, track[i].level, track[i].midiChannel);
        if (checkStepFlam(i, seqCurrentStep)) // schedule the first flam if this step is marked as flam
        {
          if (!syncStarted)
            scheduleFlam(i, track[i].accentLevel);
          else
            scheduleFlamSlave(i, track[i].accentLevel);
        }
        else // if the this step is not marked as a flam, turn off any sceduled flams
        track[i].nextFlamPulse = 0;
      }
    }
  }

  do 
  {
    if (seqNextStep >= 0)
      seqCurrentStep = (((seqCurrentStep - seqFirstStep) + seqNextStep)%seqLength) + seqFirstStep; // advance the step counter
    else // seqNextStep is negative
    {
      if ((seqCurrentStep + seqNextStep) < seqFirstStep || (seqCurrentStep + seqNextStep) > 32)
        seqCurrentStep = seqLastStep + 1 + ((seqCurrentStep - seqFirstStep) + seqNextStep);
      else
        seqCurrentStep += seqNextStep;
    }
  }
  while (checkSkip(seqCurrentStep)); // do it again if the step is marked as a skip
}
#endif


#line 1 "E:\\codez\\src\\groovesizerstochastic\\setup.ino"
void setup() 
{
  // set initial sequencer state (only sets some global vars)
  seqSetup();
  // below may rely on sequencer state
  ledsSetup();
  buttonSetup();
  potsSetup();
  clockSetup();
  midiSetup();  
  eepromSetup();
  
  // read current pot values now so that when we read them in main loop they will
  // not be perceived to change and therefore not flag just-changed indicators
  potsUpdate();

  // boot message - show version
  ledsShowNumber(VERSION_NUMBER);
  
}


#line 1 "E:\\codez\\src\\groovesizerstochastic\\ui.ino"
#if 0
void userControl()
{
  switch (mode)
  {
    // *****************************
    // *** MODE 0 - STEP ON / OFF **
    // *****************************
  case 0:
    // POTS

    if (unlockedPot(5))
    {
      if (track[currentTrack].accentLevel != pot[5] >> 3)
      {
        track[currentTrack].accentLevel = pot[5] >> 3;
        lockTimer = millis();
        number = track[currentTrack].accentLevel;
      }
    }

    if (unlockedPot(4))
    {
      if (track[currentTrack].level != pot[4] >> 3)
      {
        track[currentTrack].level = pot[4] >> 3;
        lockTimer = millis();
        number = track[currentTrack].level;
      }
    }

    if (unlockedPot(3))
    {
      byte tmp = map(pot[3], 0, 1023, 4, 48);
      if (track[currentTrack].flamDelay != tmp)
      {
        track[currentTrack].flamDelay = tmp;
        lockTimer = millis();
        number = track[currentTrack].flamDelay;
      }
    }

    if (unlockedPot(2))
    {
      byte tmp = map(pot[2], 0, 1023, (track[currentTrack].level / 2), 1);
      if (track[currentTrack].flamDecay != tmp)
      {
        track[currentTrack].flamDecay = tmp;
        lockTimer = millis();
        number = track[currentTrack].flamDecay;
      }
    }

    if (unlockedPot(1))
    {
      byte tmp = map(pot[1], 0, 1023, 0, 127);
      if (track[currentTrack].midiNoteNumber != tmp)
      {
        track[currentTrack].midiNoteNumber = tmp;
        lockTimer = millis();
        number = track[currentTrack].midiNoteNumber;
      }
    }

    if (unlockedPot(0))
    {
      byte tmp = map(pot[0], 0, 1023, 1, 16);
      if (track[currentTrack].midiChannel != tmp)
      {
        track[currentTrack].midiChannel = tmp;
        lockTimer = millis();
        number = track[currentTrack].midiChannel;
      }
    }    

    // BUTTONS
    //change mode

    if (BUTTON_JUST_PRESSED(32))
    {
      mode = 1;
      BUTTON_CLEAR_JUST_INDICATORS();
    }
    if (BUTTON_JUST_PRESSED(39))
    {
      mode = 2;
      BUTTON_CLEAR_JUST_INDICATORS();
    }
    for (byte i = 0; i < 6; i++)
    {
      if (BUTTON_JUST_PRESSED(33 + i))
      {
        if ((currentTrack < 6 && currentTrack == i) || (currentTrack > 5 && currentTrack == i + 6)) // the track is already selected
        {
          // send a note on that track
          midiA.sendNoteOff(track[currentTrack].midiNoteNumber, track[currentTrack].level, track[currentTrack].midiChannel);
          midiA.sendNoteOn(track[currentTrack].midiNoteNumber, track[currentTrack].level, track[currentTrack].midiChannel);
        }
        else // select the track
        {
          currentTrack = (currentTrack < 6) ? i : i + 6;
        }
        BUTTON_CLEAR_JUST_INDICATORS();
      }
    }
    buttonCheckSelected(); // defined in HelperFunctions
    break;      

    // *****************************
    // ***** MODE 1 - ACCENTS ******
    // *****************************
  case 1:
    if (BUTTON_JUST_RELEASED(32))
    {
      if (shiftL)
      { 
        mode = 0; // back to step on/off
        shiftMode(); // lock pots, clear just array, shift L & R = false
      }
      else
      {
        mode = 3; // change to master page (mode 3)
        shiftMode(); // lock pots, clear just array, shift L & R = false
      }  
    }
    if (BUTTON_IS_PRESSED(32)) // shift L is held
    {
      // pots
      if (unlockedPot(3)) // flamdelay according to tempo
      {
        byte tmp = map(pot[3], 0, 1023, 1, 8);
        if (track[currentTrack].flamDelay != tempoDelay(tmp))
        {
          track[currentTrack].flamDelay = tempoDelay(tmp);
          lockTimer = millis();
          number = tmp;
          shiftL = true;
        }
      }

      // buttons
      if (BUTTON_JUST_RELEASED(33)) // clear array currently being edited
      {
        clearEdit();
        shiftL = true;
        BUTTON_CLEAR_JUST_INDICATORS();
      }
      else if (BUTTON_JUST_RELEASED(39)) // clear everything
      {
        clearAll(0);
        shiftL = true;
        BUTTON_CLEAR_JUST_INDICATORS();
      }
    }

    addAccent(); // check if a button was pressed and adds an accent
    break;

    // *****************************
    // ****** MODE 2 - FLAMS *******
    // *****************************
  case 2:
    if (BUTTON_JUST_RELEASED(39))
    {
      if (!shiftR)
      {
        currentTrack = (currentTrack < 6) ? currentTrack + 6 : currentTrack - 6;
        mode = 0; 
        BUTTON_CLEAR_JUST_INDICATORS();
      }
      else
      {
        shiftR = false;
        mode = 0;
        shiftMode(); // lock pots, clear just array, shift L & R = false
      }
    }
    addFlam();
    break;


    // *****************************
    // *** MODE 3 - MASTER PAGE ****
    // *****************************
  case 3:

    // should we be displaying the playback window?
    if (windowTimer != 0 && millis() - windowTimer < 1000)
      showingWindow = true;
    else if (windowTimer != 0)
    {
      showingWindow = false;
      windowTimer = 0;
      lockPot(6);
    }

    // POTS
    if (unlockedPot(0)) // program change
    {
      if (programChange != map(pot[0], 0, 1023, 0, 127))
      {
        programChange = map(pot[0], 0, 1023, 0, 127);
        midiA.sendProgramChange(programChange, track[0].midiChannel);
        lockTimer = millis();
        number = programChange;
      }
    }

    if (unlockedPot(1)) // set the start of the playback window
    {
      if (seqFirstStep != map(pot[1], 0, 1023, 0, 32 - seqLength))
      {
        seqFirstStep = map(pot[1], 0, 1023, 0, 32 - seqLength);
        seqLastStep = seqFirstStep + (seqLength - 1);
        seqCurrentStep = seqFirstStep; // adjust the playback position
        windowTimer = millis();
        showingNumbers = false;
      }
    }

    if (unlockedPot(2)) // set the first step
    {
      if (seqFirstStep != map(pot[2], 0, 1023, 0, seqLastStep))
      {
        seqFirstStep = map(pot[2], 0, 1023, 0, seqLastStep);
        windowTimer = millis();
        seqLength = (seqLastStep - seqFirstStep) + 1;
        showingNumbers = false;
      }
    }

    if (unlockedPot(3)) // set the last step
    {
      if (seqLastStep != map(pot[3], 0, 1023, seqFirstStep, 31))
      {
        seqLastStep = map(pot[3], 0, 1023, seqFirstStep, 31);
        windowTimer = millis();
        seqLength = (seqLastStep - seqFirstStep) + 1;
        showingNumbers = false;
      }
    }

    if (unlockedPot(4)) // adjust swing
    {
      if (swing != map(pot[4], 0, 1023, 0, 255))
      {
        swing = map(pot[4], 0, 1023, 0, 255);
        lockTimer = millis();
        number = swing;
      }
    }

    if (unlockedPot(5)) // adjust bpm && clockDivider
    {
      if (BUTTON_IS_PRESSED(32))
      {
        int tmp = map(pot[5], 0, 1023, 0, 2);
        if (!syncStarted)
        {
          if (clockDivSelect != tmp)
          {
            clockDivSelect = tmp;
            scheduleNextStep();
          }
          number = clockDivSelect;
        }
        else // we're synced to clock
        {
          if (clockDivSlaveSelect != tmp)
          {
            clockDivSlaveSelect = tmp;
            scheduleNextStepSlave();
          }
          number = clockDivSlaveSelect;
        } 
        shiftL = true;
        shiftPot = true;
        lockTimer = millis();
      }
      else if (!shiftPot)
      {
        if (bpm != map(pot[5], 0, 1023, 45, 255))
        {
          bpmChange(map(pot[5], 0, 1023, 45, 255));
          lockTimer = millis();
          number = bpm;        
        }
      }
    }

    // should we be showing numbers for button presses?
    if (buttonTimer != 0 && millis() - buttonTimer < 1000)
      showingNumbers = true;
    else if (buttonTimer != 0)
    {
      showingNumbers = false;
      buttonTimer = 0;
    }

    // buttons
    if (BUTTON_IS_PRESSED(32))
    {
      if (BUTTON_JUST_PRESSED(33))
      {       
        clearEdit();
        shiftL = true;
        BUTTON_CLEAR_JUST_INDICATORS();
      }

      if (BUTTON_JUST_PRESSED(39))
      {       
        clearAll(0);
        shiftL = true;
        BUTTON_CLEAR_JUST_INDICATORS();
      }
    }

    if (BUTTON_IS_PRESSED(39))
    {
      if (BUTTON_JUST_RELEASED(32))
      {
        mode = 5; // change to references mode
        lockPot(6);
        BUTTON_CLEAR_JUST_INDICATORS();
      }
    }

    if (BUTTON_JUST_RELEASED(32))
    {
      if (shiftL) // don't change mode
      { 
        shiftMode(); // lock pots, clear just array, shift L & R = false
        shiftPot = false;
      }
      else
      {
        mode = 4; // change to trigger mode(mode 3)
        shiftMode(); // lock pots, clear just array, shift L & R = false 
      }  
    }

    if (BUTTON_JUST_PRESSED(33)) // stop/start the sequencer
    {
      if (!midiClock)
      {
        if (seqRunning)
        {
          seqRunning = false;
          seqReset();
          if (midiSyncOut)
            midiA.sendRealTime(midi::Stop); // send a midi clock stop signal)
        }
        else
        { 
          startSeq = true;
          if (midiSyncOut)
            midiA.sendRealTime(midi::Start); // send a midi clock start signal)
        }
      }
      else // midiClock is present
      {
        if (syncStarted)
        {
          syncStarted = false;
          seqReset();                  
        }
        else
        {
          syncStarted = true;
          //restartPlaySync = true;
        }
      } 
      BUTTON_CLEAR_JUST_INDICATORS();
    }

    if (BUTTON_JUST_PRESSED(34))
    {
      tapTempo();
      BUTTON_CLEAR_JUST_INDICATORS();
    }

    if (BUTTON_JUST_PRESSED(35)) // switch to mute page
    {
      mutePage = true;
      skipPage = false;
      BUTTON_CLEAR_JUST_INDICATORS();
    }

    if (BUTTON_JUST_PRESSED(36)) // switch to skip page
    {
      mutePage = false;
      skipPage = true;
      BUTTON_CLEAR_JUST_INDICATORS();
    }

    if (BUTTON_JUST_PRESSED(37)) // nudge tempo slower
    {
      if (!clockStarted)
      {
        byte tmpBPM = (bpm > 45) ? bpm - 1 : bpm; 
        bpmChange(tmpBPM);
        buttonTimer = millis();
        number = bpm;
      }
      else
      {
        if (clockCounterSlave > 0)
          clockCounterSlave--;
      }
      BUTTON_CLEAR_JUST_INDICATORS();
    }

    if (BUTTON_JUST_PRESSED(38)) // nudge tempo faster
    {
      if (!clockStarted)
      {
        byte tmpBPM = (bpm < 255) ? bpm + 1 : bpm; 
        bpmChange(tmpBPM);
        buttonTimer = millis();
        number = bpm;
      }
      else
        if (clockCounterSlave < nextStepIncomingPulse) 
          clockCounterSlave++;
      BUTTON_CLEAR_JUST_INDICATORS();
    }

    if (BUTTON_JUST_RELEASED(39)) // set trueStep - useful to flip the swing "polarity"
    {
      if (shiftR)
      {
        shiftMode();
      }
      else
      {
        seqCurrentStep = seqTrueStep;
        BUTTON_CLEAR_JUST_INDICATORS();
      }
    }

    buttonCheckSelected();

    break;

    // *****************************
    // *** MODE 4 - TRIGGER MODE ***
    // *****************************
  case 4:
    if (BUTTON_JUST_RELEASED(32))
    {
      if (shiftL) // don't change mode
      { 
        shiftMode(); // lock pots, clear just array, shift L & R = false
      }
      else
      {
        mode = 0; // change to step on/off
        shiftMode(); // lock pots, clear just array, shift L & R = false 
      }  
    }

    for (byte i = 0; i < 32; i++)
    {
      if (BUTTON_JUST_PRESSED(i))
      {
        i += (trigPage * 32); // there are 32 locations to a page
        if (i < 112) // there are only 112 save locations
        {
          if (BUTTON_IS_PRESSED(32))
          {
            if (i != nowPlaying && checkToc(i))
            {       
              clearMem = true;
              confirm = i;
              shiftL = true;
              BUTTON_CLEAR_JUST_INDICATORS();
            }
            else
            {
              shiftL = true;
              BUTTON_CLEAR_JUST_INDICATORS();
              confirm = 255;
            }  
          }
          else
          {                  
            if (i == confirm)
            {
              if (clearMem)
              {
                clearMem = false;
                tocClear(confirm);
                confirm = 255; // turns confirm off - we chose 255 because it's a value that can't be arrived at with a buttonpress
                BUTTON_CLEAR_JUST_INDICATORS();
                longPress = 0; 
              }
              else
              {
                save = true; // we're go to save a preset
                pageNum = confirm*4; // the number of the EEPROM page we want to write to is the number of the button times 4 - one memory location is 4 pages long
                tocWrite(confirm); // write a new toc that adds the current save location
                BUTTON_CLEAR_JUST_INDICATORS();
                confirm = 255; // turns confirm off                     
              }
            }
            else if (confirm == 255)
            {
              longPress = millis();
              save = false;
              recall = false;
              BUTTON_CLEAR_JUST_INDICATORS();
            }
            else
            {
              confirm = 255;
              save = false;
              recall = false;
              cancelSave = true;
              BUTTON_CLEAR_JUST_INDICATORS();
            }
          }
        }
        BUTTON_CLEAR_JUST_INDICATORS();
      }

      else if (BUTTON_IS_PRESSED(i))
      {
        i += (trigPage * 32); // multiply the trigpage by 32
        if (i < 112) // there are only 112 save locations
        {
          if (longPress != 0 && (millis() - longPress) > longPressDur) // has the button been pressed long enough
          {
            if(!checkToc(i))
            {
              save = true; // we're go to save a preset
              pageNum = i*4; // the number of the EEPROM page we want to write to is the number of the button times 4 - one memory location is 4 pages long
              tocWrite(i); // write a new toc that adds the current save location
              longPress = 0;
            }
            else // a patch is already saved in that location
            {
              confirm = i;
              longPress = 0;
            }
          }
        }
      }
      else if (BUTTON_JUST_RELEASED(i)) // we don't have to check if save is true, since we wouldn't get here if it were
      {
        i += (trigPage * 32); // multiply the trigpage by 32 and add to to i
        if (i < 112) // there are only 112 save locations
        {
          if (cancelSave)
          {
            cancelSave = false;
          }
          else if (checkToc(i) && confirm == 255)
          {
            recall = true; // we're ready to recall a preset
            pageNum = i*4; // one memory location is 4 pages long
            BUTTON_CLEAR_JUST_INDICATORS();
            if (midiTrigger)
            {
              midiA.sendNoteOn(i, 127, triggerChannel); // send a pattern trigger note on triggerChannel (default is channel 10)
            }
            cued = i; // the pattern that will play next is i - need this to blink lights
            head = i; // the first pattern in a series of chained patterns
            followAction = 0; // allow the next loaded patch to prescribe to followAction
          }
        }
      }  
    } 
    if (BUTTON_IS_PRESSED(32))
    {
      if (BUTTON_JUST_PRESSED(33))
      {
        trigPage = 0;
        BUTTON_CLEAR_JUST_INDICATORS();
        shiftL = true;    
      }  
      else if (BUTTON_JUST_PRESSED(34))
      {
        trigPage = 1;
        BUTTON_CLEAR_JUST_INDICATORS();
        shiftL = true;  
      }
      else if (BUTTON_JUST_PRESSED(35))
      {
        trigPage = 2;
        BUTTON_CLEAR_JUST_INDICATORS();
        shiftL = true;  
      }
      else if (BUTTON_JUST_PRESSED(36))
      {
        trigPage = 3;
        BUTTON_CLEAR_JUST_INDICATORS();
        shiftL = true;
      }
      else if (BUTTON_JUST_PRESSED(37)) // set followAction to 2 - return to the head
      {
        if (followAction != 2)
        {
          followAction = 2;
          //saved = false; // these follow actions only become active once the patch has been saved and loaded
          shiftL = true;
        }
        else
        {
          followAction = 0;
          shiftL = true;
        }
        save = true; // save the patch with its new followAction
        BUTTON_CLEAR_JUST_INDICATORS();
      }
      else if (BUTTON_JUST_PRESSED(38)) // set followAction to 1 - play the next pattern
      {
        if (followAction != 1)
        {
          followAction = 1;
          saved = false; // these follow actions only become active once the patch has been saved and loaded
          shiftL = true;
        }
        else
        {
          followAction = 0;
          shiftL = true;
        }
        save = true; // save the patch with its new followAction 
        BUTTON_CLEAR_JUST_INDICATORS();
      }
    }
    else if (BUTTON_IS_PRESSED(33)) // step repeat
    {
      seqNextStep = 0; 
      BUTTON_CLEAR_JUST_INDICATORS();
    }
    else if (BUTTON_JUST_RELEASED(33))
    {
      seqNextStep = 1;
      seqCurrentStep = seqTrueStep;
      BUTTON_CLEAR_JUST_INDICATORS();
    }
    else if (BUTTON_IS_PRESSED(34)) // momentary reverse
    {
      seqNextStep = -1; 
      BUTTON_CLEAR_JUST_INDICATORS();
    }
    else if (BUTTON_JUST_RELEASED(34))
    {
      seqNextStep = 1;
      seqCurrentStep = seqTrueStep;
      BUTTON_CLEAR_JUST_INDICATORS();
    }

    else if (BUTTON_JUST_PRESSED(35))
    {
      fxFlamSet();
      fxFlamDelay = (clockDiv[clockDivSelect] * 4) / 3;
      fxFlamDecay = 10;
      BUTTON_CLEAR_JUST_INDICATORS();
    }


    if(save)
      savePatch();

    break;

    // *****************************
    // *** MODE 5 - PREFERENCES  ***
    // *****************************
  case 5:
    showingNumbers = false;

    // buttons
    if (BUTTON_JUST_PRESSED(32))
    {
      mode = 3;
      shiftL = true;
      savePrefs(); // save the preferences on exit
    }

    if (BUTTON_JUST_PRESSED(39))
    {
      mode = 3;
      shiftR = true;
      savePrefs(); // save the preferences on exit
    }

    if (BUTTON_JUST_PRESSED(33))
    {
      midiSyncOut = !midiSyncOut;
      BUTTON_CLEAR_JUST_INDICATORS();
    }

    if (BUTTON_IS_PRESSED(34))
    {
      showingNumbers = true;
      number = thruOn;
      if (unlockedPot(5)) // adjust midi echo mode
      {
        if (thruOn != map(pot[5], 0, 1023, 0, 2))
        {
          thruOn = map(pot[5], 0, 1023, 0, 2);
          lockTimer = millis();
        }
      }
    }

    if (BUTTON_IS_PRESSED(35))
    {
      showingNumbers = true;
      number = triggerChannel;
      if (unlockedPot(5)) // adjust trigger channel
      {
        if (triggerChannel != map(pot[5], 0, 1023, 0, 16))
        {
          triggerChannel = map(pot[5], 0, 1023, 0, 16);
          midiTrigger = (triggerChannel == 0) ? false : true;
          lockTimer = millis();
        }
      }
    }

    break;
  }
}
#endif


void uiAcceptInput(void)
{
  // TODO main ui loop
}




