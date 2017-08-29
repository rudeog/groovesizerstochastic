#include "avr_main.h"
// for the rows of LEDs
// the byte that will be shifted out 

// time in ms to display temporary values (like if a pot is twiddled)
#define LED_TEMP_DISPLAY_TIME 1000
// time in milliseconds between state flip
#define LED_BLINK_TIME 200

#define LED_BYTES 5
byte LEDrow[LED_BYTES];
// bits are turned on here if they are blinking
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


// called to initialize LED's
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
  memset(LEDblink,0,LED_BYTES);
  
}

// determines what bits need to be lit based on blink state and
// blink bits. if blinkcycle is set, all blink bits will be on
// otherwise off. After this, any blink bits, or any regular on bits
// make the led light. This means if something has on bit and also
// a blink bit, it will always be on. It must have only the blink bit
// to be blinking
#define RESOLVE_BITS(led, blink)   (blink & gBlinkCycle) | led

// @AS this makes no sense to me... needs to be called twice to
// see the screen and then the led's are on 50% duty cycle. is it really necessary? 
// this function shifts out the the 4 bytes corresponding to the led rows
static void ledsLightUp(void)
{
   static boolean lastSentTop = false;
   
   // we want to alternate sending the top 2 and bottom 3 rows to prevent an edge 
   // case where 4 rows of LEDs lit at the same time sourcing too much current
  
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
      } else
         return; // keep showing number
   }

  // full update based on current state
  ledsFullUpdate();
  
  // light the actual lights
  ledsLightUp();
}

// this is used when we are not displaying a temporary value.
// it does a full update based on the current state of things
static void ledsFullUpdate()
{
   // start with all off
   ledsOff();
   
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
