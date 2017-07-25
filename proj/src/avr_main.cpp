#include "avr_main.h"

//
// Globals
//
uint8_t gButtonJustChanged[BUTTON_BYTES];
uint8_t gButtonIsPressed[BUTTON_BYTES];
uint16_t gPotValue[POT_COUNT];  // to store the values of out 6 pots (range 0-1023)
uint8_t gPotChangeFlag;         // determine whether pot has changed

//
// Arduino calls setup
//
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
  
  // initial clock setup (get the timer running)
  seqSetBPM(DEFAULT_BPM); 
  
  // boot message - show version
  ledsShowNumber(VERSION_NUMBER);
  
}

//
// Arduino calls main loop
//
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
