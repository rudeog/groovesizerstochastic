#include "avr_main.h"

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
buttonSetup(void)
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

void buttonsUpdate(void)
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
  //Collect button data from the 4901s
  //Pulse the latch pin:
  //set it to 1 to collect parallel data
  digitalWrite(BUTTONlatchPin,1);
  //set it to 1 to collect parallel data, wait
  delayMicroseconds(20);
  //set it to 0 to transmit data serially  
  digitalWrite(BUTTONlatchPin,0);  
  

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

