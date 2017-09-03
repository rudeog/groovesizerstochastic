
/* Handle the "UI". ie handle when buttons are pressed and pots are turned.
 * Switch between modes, or update settings
 */

#include "avr_main.h"

/* This handles switching between modes
 */
static 
void modeCheck(void)
{
   uint8_t i;
   switch(gRunningState.currentMode) {
   case SEQ_MODE_NONE: // can switch to any mode except stepedit
      if(POT_MAP(POT_1, SEQ_TRED_OFF, SEQ_TRED_TOTAL-1) > SEQ_TRED_OFF || 
         POT_MAP(POT_5, SEQ_GLBL_OFF, SEQ_GLBL_TOTAL-1) > SEQ_GLBL_OFF) {
         // switch to POTEDIT mode
         gRunningState.currentMode=SEQ_MODE_POTEDIT;
         gRunningState.currentSubMode=0; // will be set anyway in uiHandlePotMode
      } else if(BUTTON_IS_PRESSED(BUTTON_L_SHIFT)) {
         gRunningState.currentMode=SEQ_MODE_LEFT;
      } else if(BUTTON_IS_PRESSED(BUTTON_R_SHIFT)) {
         gRunningState.currentMode=SEQ_MODE_RIGHT;
      }
      break;
   case SEQ_MODE_POTEDIT: // can shift to none when both pots are at 0
      if(POT_MAP(POT_1, SEQ_TRED_OFF, SEQ_TRED_TOTAL-1) == SEQ_TRED_OFF &&
         POT_MAP(POT_5, SEQ_GLBL_OFF, SEQ_GLBL_TOTAL-1) == SEQ_GLBL_OFF) {
         gRunningState.currentMode=SEQ_MODE_NONE;
      }   
      break;
   case SEQ_MODE_STEPEDIT: // can shift to none when l-shift is JUST pressed
      if(BUTTON_JUST_PRESSED(BUTTON_L_SHIFT)) 
         gRunningState.currentMode=SEQ_MODE_NONE;
      
      break;
   case SEQ_MODE_LEFT: 
      // can shift to none when released
      if(!BUTTON_IS_PRESSED(BUTTON_L_SHIFT)) {
         gRunningState.currentMode=SEQ_MODE_NONE;
      } else {
         // can shift to stepedit when a green button is pressed
         // we check for it's release rather because once in stepedit mode, we check for a press
         for(i=0;i<BUTTON_L_SHIFT;i++) { // for all green buttons
            if(BUTTON_JUST_RELEASED(i)) {
               // set submode to the step being edited
               gRunningState.currentSubMode=i;
               gRunningState.currentMode=SEQ_MODE_STEPEDIT;
               break;
            }
         }         
      }
      break;
   case SEQ_MODE_RIGHT: // can shift to none when released
      if(!BUTTON_IS_PRESSED(BUTTON_R_SHIFT)) {
         gRunningState.currentMode=SEQ_MODE_NONE;
         gRunningState.currentSubMode=0; // indicates selected next-pattern or num cycles
      }
      break;   
   }
}

/* handles mode NONE. When we are not in a specific mode
 * need to respond to buttons which turn on and off notes
 * and also respond to F buttons which switch tracks
 * also modify tempo
 */
static void 
uiHandleMainMode()
{
   uint8_t i, len;
   // remember: numSteps is 0 based
   len=gSeqState.tracks[gRunningState.currentTrack].numSteps+1;
   SeqStep *steps=gSeqState.patterns[gRunningState.pattern].trackSteps[gRunningState.currentTrack].steps;
   
   // check to see if any of the step buttons were just pressed and  toggle them
   for(i=0;i<len;i++) {
      if (BUTTON_JUST_PRESSED(i)) {
         if (!steps[i].velocity) {// currently off
            steps[i].velocity = 15;
            steps[i].probability = 15;
         }
         else // turn off
            steps[i].velocity = 0;      
      }
   }

   // check to see if any F buttons were just pressed and switch pattern
   for (i = BUTTON_FUNCTION; i < BUTTON_FUNCTION + BUTTON_NUM_FUNCTION; i++) {
      if (BUTTON_JUST_PRESSED(i)) {
         // if the track is not current, switch to it
         gRunningState.currentTrack = i-BUTTON_FUNCTION;
      }
   }
   
   if(POT_JUST_CHANGED(POT_3)) {      
      // tempo can be 0, or 50-255
      gSeqState.tempo = POT_MAP(POT_3,49,255);
      if(gSeqState.tempo==49)
         gSeqState.tempo=0;      
      ledsShowNumber(gSeqState.tempo);
   }
   
}

/* if in pot mode, we will respond to pot 1, 2, 5 and 6
 * if 1 or 5 are set to non-zero, we set a new submode
 * if 2 or 6 is moved, we check submode and alter setting based on it
 * If either of these two are moved while left-shift is down, we just display the value
 * some of the modes are y/n which means that a button press toggles it
 */
static void
uiHandlePotMode(void)
{
   uint8_t pv;
   uint8_t showOnly=0; // 1=don't alter the value, 2=also display the value
   uint8_t showVal=0;
   uint8_t global;
   SeqTrack *track = &gSeqState.tracks[gRunningState.currentTrack];

   // set the correct sub-mode based on pots 1 and 5

   // see if we are in global edit
   pv = POT_MAP(POT_5, SEQ_GLBL_OFF, SEQ_GLBL_TOTAL-1);
   if (pv != SEQ_GLBL_OFF) {
      global = 1;
   } else {
      // see if we are in track edit
      pv = POT_MAP(POT_1, SEQ_TRED_OFF, SEQ_TRED_TOTAL-1);      
      global=0;
   }
      
   if (!pv)
      return; // this shouldn't happen (see modeCheck) - when both are 0

   // The submode is set to whichever setting is being edited
   gRunningState.currentSubMode = pv;
   
   if(global) // signify that we are editing a global setting
      gRunningState.currentSubMode |= SEQ_GLBL_OR_TRED_SELECTOR;
      
   if(BUTTON_IS_PRESSED(BUTTON_L_SHIFT))
      showOnly=1; // show value only, don't alter it

   if (!global && POT_JUST_CHANGED(POT_2)) { 
      // if one of the track edit modes is set apply it's new setting to the track 
      // also display the value
      switch(pv) {
      case SEQ_TRED_CHANNEL: // 0 based midi channel
         if(!showOnly)
            track->midiChannel=POT_MAP(POT_2, 0, 15);
         showVal=track->midiChannel+1;
         break;
      case SEQ_TRED_NOTENUM: // midi note
         if(!showOnly)
            track->midiNote=POT_MAP(POT_2,0,127);
         showVal=track->midiNote;
         break;
      case SEQ_TRED_MUTED:   // muted (y/n)
         if(!showOnly)
            track->muted=POT_MAP(POT_2,0,1);
         showVal=track->muted;
         break;
      case SEQ_TRED_NUMERATOR: // 1..4 (0 based)
         if(!showOnly)
            track->clockDividerNum = POT_MAP(POT_2,0,3);
         showVal=track->clockDividerNum+1;
         break;
      case SEQ_TRED_DENOM: // 1..4 (0 based)
         if(!showOnly)
            track->clockDividerDenom = POT_MAP(POT_2,0,3);
         showVal=track->clockDividerDenom+1;
         break;
      case SEQ_TRED_STEPCOUNT: // 1..32 (0 based)
         if(!showOnly)
            track->numSteps = POT_MAP(POT_2,0,31);
         showVal=track->numSteps+1;
         break;
      default:
         break;
      }
      showOnly=2; // display the value (see below)
   }   
   else if (global && POT_JUST_CHANGED(POT_6)) { // editing global
      // if one of the global edit modes, apply it
      switch(pv) {
      case SEQ_GLBL_SWING:    // edit swing
         if(!showOnly)
            gSeqState.swing = POT_MAP(POT_6,0,100);
         showVal=gSeqState.swing;
         break;
      case SEQ_GLBL_RANDREGEN:// edit random regen (gen new random number every n steps) 1 based (1-32)
         if(!showOnly)
            gSeqState.randomRegen = POT_MAP(POT_6,1,32);
         showVal=gSeqState.randomRegen;
         break;
      case SEQ_GLBL_DELAYPAT: // delay pattern switch (y/n)
         if(!showOnly)
            gSeqState.delayPatternSwitch = POT_MAP(POT_6,0,1);
         showVal=gSeqState.delayPatternSwitch;
         break;
      case SEQ_GLBL_SENDCLK:  // send midi clock (y/n)
         if(!showOnly)
            gSeqState.midiSendClock = POT_MAP(POT_6,0,1);
         showVal=gSeqState.midiSendClock;
         break;
      case SEQ_GLBL_SENDTHRU: // send midi thru (y/n)
      {
         uint8_t t = POT_MAP(POT_6,0,1);         
         if(!showOnly && gSeqState.midiThru != t) {
            gSeqState.midiThru = t;
            midiSetThru();
         }
         showVal=gSeqState.midiThru;
         break;
      }
      default:
         break;
      }
      showOnly=2; // display the value (see below)
   }
   else if (BUTTON_JUST_PRESSED(BUTTON_TOGGLE_SETTING)) {
      // if in a mode that has a toggle setting, toggle it
      if(!global) {
         if(pv==SEQ_TRED_MUTED){   // muted (y/n)
            track->muted=!track->muted;         
         }
      } else {
         switch(pv) {
         case SEQ_GLBL_DELAYPAT: // delay pattern switch (y/n)
            gSeqState.delayPatternSwitch = !gSeqState.delayPatternSwitch;
            break;
         case SEQ_GLBL_SENDCLK:  // send midi clock (y/n)
            gSeqState.midiSendClock = !gSeqState.midiSendClock;
            break;
         case SEQ_GLBL_SENDTHRU: // send midi thru (y/n)
            gSeqState.midiThru = !gSeqState.midiThru;      
            midiSetThru(); 
            break;
         default:
            break;
         }      
      }
   }
   
   if(showOnly==2)
      ledsShowNumber(showVal);
}

/* In step edit mode, we have selected a step for editing, so our submode indicates
 * which step was selected for editing. 
 * We will respond to buttons on the top two rows to set velocity (where the first button sets it to 0)
 * Bottom two rows will set probability, where the first button sets it to least-likely
 */
static void
uiHandleStepEdit()
{
   uint8_t i;
   for(i=0;i<32;i++) { // top 4 rows
      if(BUTTON_JUST_PRESSED(i)) {
         SeqTrackStep *trs = &gSeqState.patterns[gRunningState.pattern].trackSteps[gRunningState.currentTrack];
         if(i < 16) { // set velocity
            trs->steps[gRunningState.currentSubMode].velocity=i;
         } else { // set probability
            trs->steps[gRunningState.currentSubMode].probability=i;
         }
         break;
      }         
   }
}

/* In this mode we can toggle mute on a track, 
 * also exercising pot 3 shows current tempo without changing it
 * Also, if R shift is pressed we toggle playback
*/
static void
uiHandleLeftMode()
{
   uint8_t i;
   SeqTrack *track;
   for (i = BUTTON_FUNCTION; i < BUTTON_FUNCTION + BUTTON_NUM_FUNCTION; i++) {
      if (BUTTON_JUST_PRESSED(i)) {
          track = &gSeqState.tracks[i-BUTTON_FUNCTION];
         // mute/unmute
         track->muted=!track->muted;
         break;
      }
   }
   
   // handle tempo display
   if(POT_JUST_CHANGED(POT_3)) {      
      ledsShowNumber(gSeqState.tempo);
   }
   
   // check r-shift and toggle playback
   if(BUTTON_JUST_PRESSED(BUTTON_R_SHIFT)) {      
      seqSetTransportState(gRunningState.transportState==TRANSPORT_STARTED ? TRANSPORT_STOPPED : TRANSPORT_STARTED);
   }
   
}

/* While R-Shift is held:
 *    F button 1..4 selects the pattern.
 *    F6 toggles pattern hold
 *    L-shift starts/stops playback
 *    1..4 of bottom row of green buttons selects submode for which pattern to edit probability or whether to
 *      edit numcycles
 *    Top two rows of green buttons set probability or numcycles depending on submode  
*/
static void
uiHandleRightMode()
{
   // If F 1..4 is pressed, select current pattern
   uint8_t i;
   for(i=BUTTON_FUNCTION;i<BUTTON_FUNCTION+4;i++) {
      if(BUTTON_JUST_PRESSED(i)) {
         seqSetPattern(i - BUTTON_FUNCTION);
         break;
      }         
   }
   
   // bottom row of green buttons (1..4) selects pattern for editing (submode),
   // or if 5 is pressed, selects num cycles
   for(i=24;i<24+5;i++) {
      if(BUTTON_JUST_PRESSED(i)) {
         // submode is the 0 based pattern number we are editing probability for
         // or num cycles if set to 4
         gRunningState.currentSubMode=i-24;
         break;
      }         
   }
   
   // top two row of buttons are for setting either the probability or the num cycles
   // depending on current sub mode
   for(i=0;i<16;i++) {
      if(BUTTON_JUST_PRESSED(i)){
         SeqPattern *statePattern = &gSeqState.patterns[gRunningState.pattern];
         
         if(gRunningState.currentSubMode < 4) { //0..3
            // it is 0..3, set pattern prob
            uint8_t pp=gRunningState.currentSubMode;
            uint8_t idx = pp/2; 
            if (pp % 2 == 0) { // low nibble
               statePattern->nextPatternProb[idx] &= 0xF0; // clear low nibble
               statePattern->nextPatternProb[idx] |= i; // set it
            }
            else { // high nibble
               statePattern->nextPatternProb[idx] &= 0x0F; // clear high nibble
               statePattern->nextPatternProb[idx] |= (i << 4); // set it
            }                        
         } else { // it is equal to 4 (which represents numcycles)
            // set num cycles
            statePattern->numCycles = i;
         }
         break;
      }
   }
   
   // If F6 is pressed, we toggle "pattern hold"
   if(BUTTON_JUST_PRESSED(BUTTON_FUNCTION+5)) {
      gRunningState.patternHold = !gRunningState.patternHold;
   }
      
   // check L-shift and toggle playback
   if(BUTTON_JUST_PRESSED(BUTTON_L_SHIFT)) {      
      seqSetTransportState(gRunningState.transportState==TRANSPORT_STARTED ? TRANSPORT_STOPPED : TRANSPORT_STARTED);
   }
}

/* Main dispatcher for user interaction. Try to keep things modularized and single purpose here
*/
void
uiAcceptInput(void)
{
   // mode state machine. see if we need to shift modes
   modeCheck();
   
   // handle current mode
   switch(gRunningState.currentMode) {
   case SEQ_MODE_NONE:
      uiHandleMainMode();
      break;
   case SEQ_MODE_POTEDIT:
      uiHandlePotMode();
      break;
   case SEQ_MODE_STEPEDIT:
      uiHandleStepEdit();
      break;
   case SEQ_MODE_LEFT:
      uiHandleLeftMode();
      break;
   case SEQ_MODE_RIGHT:
      uiHandleRightMode();
      break;   
   }
}


