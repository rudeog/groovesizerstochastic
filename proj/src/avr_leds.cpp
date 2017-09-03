#include "avr_main.h"

// time in ms to display temporary values (like if a pot is twiddled)
#define LED_TEMP_DISPLAY_TIME 1500
// time in milliseconds between state flip
#define LED_BLINK_TIME 200


#define LED_BYTES 5

// for the rows of LEDs
// the byte that will be shifted out 
// @AS the least significant bit of each of these items is the left most LED of that row
//     MOST SIGNIFICANT BIT IS THE LED ON THE RIGHT!
byte LEDrow[LED_BYTES];
// bits are turned on here if they are blinking (must not have corresponding LEDrow bit turned on, otherwise it will be solid lit)
byte LEDblink[LED_BYTES];

// will be set to true if we are currently showing a temporary value on the display
uint8_t  gShowingNumber;
uint16_t gNumberTimer; // timer for temporary numeric value
uint16_t gBlinkTimer; // timer for blinking leds
uint8_t  gBlinkCycle; // will be 1 if blink on, 0 if blink off

// setup code for controlling the LEDs via 74HC595 serial to parallel shift registers
// based on ShiftOut http://arduino.cc/en/Tutorial/ShiftOut
const byte LEDlatchPin = 2;
const byte LEDclockPin = 6;
const byte LEDdataPin = 4;

static void ledsFullUpdate();


/* ShiftOut to address 42 LEDs via 4 74HC595s serial to parallel shifting registers 
 * Based on http://arduino.cc/en/Tutorial/ShiftOut
 */
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
  memset(LEDblink,0,LED_BYTES);
  
}

// determines what bits need to be lit based on blink state and
// blink bits. if blinkcycle is set, all blink bits will be on
// otherwise off. After this, any blink bits, or any regular on bits
// make the led light. This means if something has on bit and also
// a blink bit, it will always be on. It must have only the blink bit
// to be blinking
#define RESOLVE_BITS(led, blink)   (blink & gBlinkCycle) | led

// this function shifts out the the 4 bytes corresponding to the led rows
static void ledsLightUp(void)
{
   static boolean lastSentTop = false;

   // we want to alternate sending the top 2 and bottom 3 rows to prevent an edge 
   // case where 4 rows of LEDs lit at the same time sourcing too much current
   //
   // @AS this makes no sense to me... needs to be called twice to
   // see the screen and then the led's are on 50% duty cycle. is it really necessary? 

   
   // blink on or off
   if((int16_t)((uint16_t)millis()-gBlinkTimer) > LED_BLINK_TIME) {
      if(gBlinkCycle)
         gBlinkCycle = 0x00;
      else
         gBlinkCycle = 0xFF;
      gBlinkTimer=millis();
   }
  
   // ground LEDlatchPin and hold low for as long as you are transmitting
   digitalWrite(LEDlatchPin, 0);
   if (!lastSentTop) { // send the top to rows
      shiftOut(LEDdataPin, LEDclockPin, B00000000);
      shiftOut(LEDdataPin, LEDclockPin, B00000000); 
      shiftOut(LEDdataPin, LEDclockPin, B00000000);
      shiftOut(LEDdataPin, LEDclockPin, RESOLVE_BITS(LEDrow[1], LEDblink[1])); 
      shiftOut(LEDdataPin, LEDclockPin, RESOLVE_BITS(LEDrow[0], LEDblink[0]));
      lastSentTop = true;
   } else { // ie. lastSentTop is true, then send the bottom 3 rows
      shiftOut(LEDdataPin, LEDclockPin, RESOLVE_BITS(LEDrow[4],LEDblink[4]));
      shiftOut(LEDdataPin, LEDclockPin, RESOLVE_BITS(LEDrow[3],LEDblink[3])); 
      shiftOut(LEDdataPin, LEDclockPin, RESOLVE_BITS(LEDrow[2],LEDblink[2]));
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
    LEDrow[3] = B00000001;  
  if (number > 99) // add a dot for numbers 100 and over
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
  gShowingNumber=1;
  gNumberTimer=millis();
}

// called from main loop
void ledsUpdate(void)
{
   // if we are displaying a temporary value, just let it abide
   if(gShowingNumber) {
      // if enough time has passed...
      if((int16_t)((uint16_t)millis()-gNumberTimer) > LED_TEMP_DISPLAY_TIME) {
         // stop showing it
         gShowingNumber=0;
      }
   }

  // full update based on current state
  if(!gShowingNumber)
   ledsFullUpdate();
  
  // light the actual lights
  ledsLightUp();
}

/* called from setup to initialize LED's
 */
void 
ledsSetup(void)
{
  //define pin modes 
  pinMode(LEDlatchPin, OUTPUT);
  pinMode(LEDclockPin, OUTPUT); 
  pinMode(LEDdataPin, OUTPUT);
  gNumberTimer=0;
  gShowingNumber=0;
}

/* In NONE mode (main mode), we are going to highlight the steps that are on, as well as the
 * current step that's playing (if playing)
 * We are also going to highlight the current track on the F buttons
 */
static void
ledsHandleMainMode()
{
   uint8_t i;
   SeqTrack *tr = &gSeqState.tracks[gRunningState.currentTrack];
   SeqTrackStep *trs = &gSeqState.patterns[gRunningState.pattern].trackSteps[gRunningState.currentTrack];
   
   // highlight all turned on steps (numsteps is 0 based)
   for(i=0;i<tr->numSteps+1;i++) {
      if (trs->steps[i].velocity) {
         bitSet(LEDrow[i / 8], i % 8); 
      }
   }

   // if rolling, toggle current step so it's noticeable
   if(gRunningState.transportState==TRANSPORT_STARTED) {
      TrackRunningState *t=&gRunningState.trackStates[gRunningState.currentTrack];
      // flip it off if it's on, on if it's off
      LEDrow[t->position / 8] ^=  (1 << (t->position % 8));      
   }
   
   // highlight active track on the bottom row (remember first led on left is on the shift
   // button, so we need to start with the second one over for track 1)
   bitSet(LEDrow[4], gRunningState.currentTrack + 1);
   
   
}

/* In pot mode, we need to display an indicator for the currently selected option
 * which will either be a global option or a track option. The global options will
 * be indicated by the first green row (5 options), the track options by the second row
 * (6 options)
 * Some of the options are toggle. For these, the value will be displayed with the top
 * right LED (BUTTON_TOGGLE_SETTING button) which will blink if NO and solid if YES
 * When pots that alter value of options are twiddled a number is shown with the value.
 * That is not handled here, but in uiHandlePotMode.
 */
static void
ledsHandlePotMode()
{
   uint8_t global=0;
   uint8_t pv;
   uint8_t boolval=0; // default not boolean, 1=boolean on, 2=boolean off
   global = (gRunningState.currentSubMode & SEQ_GLBL_OR_TRED_SELECTOR) != 0;
   pv=gRunningState.currentSubMode & (~SEQ_GLBL_OR_TRED_SELECTOR);
   
   if(!global) {
      SeqTrack *track = &gSeqState.tracks[gRunningState.currentTrack];
      switch(pv) {
      case SEQ_TRED_CHANNEL: LEDrow[0] = B00000001; break;
      case SEQ_TRED_NOTENUM: LEDrow[0] = B00000010; break;
      case SEQ_TRED_MUTED:
         LEDrow[0] = B00000100; 
         boolval=(track->muted ? 1 : 2);
         break;
      case SEQ_TRED_NUMERATOR: LEDrow[0]=B00001000; break;
      case SEQ_TRED_DENOM: LEDrow[0] =   B00010000; break;
      case SEQ_TRED_STEPCOUNT: LEDrow[0]=B00100000; break;
      default: 
         LEDrow[0] = B01111111; 
         break; // something wrong!
      }      
   } else { // global mode
      switch(pv) {
      case SEQ_GLBL_SWING:     LEDrow[1] = B00000001; break;
      case SEQ_GLBL_RANDREGEN: LEDrow[1] = B00000010; break;
      case SEQ_GLBL_DELAYPAT:  
         LEDrow[1] = B00000100; 
         boolval=(gSeqState.delayPatternSwitch ? 1 : 2);
         break;
      case SEQ_GLBL_SENDCLK:   
         LEDrow[1] = B00001000; 
         boolval=(gSeqState.midiSendClock ? 1 : 2);
         break;
      case SEQ_GLBL_SENDTHRU:  
         LEDrow[1] = B00010000; 
         boolval=(gSeqState.midiThru ? 1 : 2);
         break;
      default:
         LEDrow[1] = B11111111; 
         break; // something wrong!
      }      
   }   
   
   // show boolean types if applicable in top right LED
   if(boolval) {
      if(boolval==1) // on -- show solid
         bitSet(LEDrow[0],7);
      else // off -- show blinking
         bitSet(LEDblink[0],7);
   
   }   
}

/* Create a bar graph on specified row that wraps around to following row.
   Value is expected to be 0..31, but will be converted to 1..32 so that a
   value of 0 will have 1 LED lit and a value of 15 will have 16 LED's lit.
 */
static void
setBarGraph(uint8_t rowStart, uint8_t value)
{
   uint8_t val = value+1;
   uint8_t bval=0;
   
   if(val > 24) {
      val -=8;
      // first row is all lit
      LEDrow[rowStart] = B11111111;
      // row we set is second row
      rowStart++;
   }
   
   if(val > 16) {
      val -=8;
      // first row is all lit
      LEDrow[rowStart] = B11111111;
      // row we set is second row
      rowStart++;
   }
   
   if(val > 8) {
      val -=8;
      // first row is all lit
      LEDrow[rowStart] = B11111111;
      // row we set is second row
      rowStart++;
   }
   
   // val will now be 1-8
   for(uint8_t i=0;i<val;i++) {
      // set bits starting at LSB
      bval <<= 1;
      bval |=0x01;
   }
   
   LEDrow[rowStart]=bval;
}

/* Edited step is flashing (submode)
 * Top two rows indicate velocity as a bar graph
 * Bottom two rows indicate probability as a bar graph
 * L shift is blinking (to indicate that if pressed, the mode will be exited)
 */
static void 
ledsHandleStepEdit()
{
   SeqStep *steps=gSeqState.patterns[gRunningState.pattern].trackSteps[gRunningState.currentTrack].steps;
   
   // display velocity as a bar graph on row 0 and 1
   setBarGraph(0,steps[gRunningState.currentSubMode].velocity);   
   setBarGraph(2,steps[gRunningState.currentSubMode].probability);
   
   // blink the edited step
   bitSet(LEDblink[gRunningState.currentSubMode/8], gRunningState.currentSubMode % 8);
   bitClear(LEDrow[gRunningState.currentSubMode/8], gRunningState.currentSubMode % 8);
   
   // blink the l-shift
   LEDblink[4]=B00000001;
   
}

/* total number of steps in track will light
 * unmuted tracks (F lights) will be lit
 */
static void
ledsHandleLeftMode()
{
   // display which steps are available for selecting
   setBarGraph(0,gSeqState.tracks[gRunningState.currentTrack].numSteps);
   
   // display which tracks are not muted
   for(int i=0;i<NUM_TRACKS;i++) {
      if(!gSeqState.tracks[i].muted) {
         bitSet(LEDrow[4],i+1); 
      }
   }
}

/* F light 1..4 will light with current pattern
 * F6 will be lit if pattern hold is in effect
 * Bottom row left 5 lights are lit and one is blinking (depending on whether editing pat prob 1..4 or num cycles)
 * top two rows indicate as a bar graph the value of current pat prob or num cycles
 */
static void
ledsHandleRightMode()
{
   uint8_t v;
   // F row shows current pattern 0..3
   bitSet(LEDrow[4],gRunningState.pattern+1);
   
   // light F6 if pattern hold
   if(gRunningState.patternHold)
      bitSet(LEDrow[4], 6);
   
   // light the currently selected submode on 4th row
   bitSet(LEDrow[3],gRunningState.currentSubMode);
   
   // show the current value depending on submode
   if(gRunningState.currentSubMode < 4) { 
      // pattern 0..3 selected, show probability (0..15)
      // 0 means no probability of switching to that pattern, which will be indicated with 1 light lit
      v=gSeqState.patterns[gRunningState.pattern].nextPatternProb[gRunningState.currentSubMode/2];
      if(gRunningState.currentSubMode % 2 == 0) // first or 3rd
         v &= 0x0F;
      else
         v >>= 4;
   } else {
      // 4 is selected, which means numcycles to play current pattern (0 based)
      v=gSeqState.patterns[gRunningState.pattern].numCycles;
   }
   
   setBarGraph(0,v);
   
}


// this is used when we are not displaying a temporary value.
// it does a full update based on the current state of things
static void ledsFullUpdate()
{
   // start with all off
   ledsOff();
   
   // determine current mode, and display led's accordingly
   switch(gRunningState.currentMode) {
   case SEQ_MODE_NONE:
      ledsHandleMainMode();
      break;
   case SEQ_MODE_POTEDIT:
      ledsHandlePotMode();
      break;
   case SEQ_MODE_STEPEDIT:
      ledsHandleStepEdit();
      break;
   case SEQ_MODE_LEFT:
      ledsHandleLeftMode();
      break;
   case SEQ_MODE_RIGHT:
      ledsHandleRightMode();
      break;   
   }
   
   
}

