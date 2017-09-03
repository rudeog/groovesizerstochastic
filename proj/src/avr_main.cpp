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
   uint8_t seq=0, seed=0, i;

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
  
  // check pots 1 and 5 to make sure they are at 0, if not
  // display a warning and wait. this also gets a random seed
   if(POT_VALUE(POT_1) > 5 || POT_VALUE(POT_5) > 5) {
      for(;;) {
         seed++;
         
         // if both are now 0 we are done
         if(POT_VALUE(POT_1) < 5 && POT_VALUE(POT_5) < 5)
            break;
         
         for(i=0;i<POT_COUNT;i++) {
            if(POT_JUST_CHANGED(i))
               seq=i;
         }
         
         buttonsUpdate();
         potsUpdate();
         // display some figure to indicate pots need to be set to 0
         ledsShowNumber(15);
         ledsUpdate();
      }
   } else {      
      // wait for user to press a button or move a pot.
      // this allows us to seed our random number gen with something truly random
      for(;;) {
         for(i=0;i < BUTTON_COUNT;i++) {
            if(BUTTON_JUST_PRESSED(i)) {
               seq=i;
               goto end;
            }
         }
         
         for(i=0;i<POT_COUNT;i++) {
            if(POT_JUST_CHANGED(i)) {
               seq=i;
               goto end;
            }
         }
         seed++;
         buttonsUpdate();
         potsUpdate();
         ledsUpdate();
      }
   }
   
end:
   // show the seed
   ledsShowNumber(seed);
   // seed the rng
   rndSRandom( seed, seq);
  
  
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
