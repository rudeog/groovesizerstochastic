#include "avr_main.h"

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


static 
void modeCheck(void)
{
   switch(gRunningState.currentMode) {
   case SEQ_MODE_NONE: // can switch to any mode except stepedit
      if(POT_MAP(POT_1, SEQ_TRED_OFF, SEQ_TRED_TOTAL) > SEQ_TRED_OFF || 
         POT_MAP(POT_5, SEQ_GLBL_OFF, SEQ_GLBL_TOTAL) > SEQ_GLBL_OFF) {
         // switch to POTEDIT mode
         gRunningState.currentMode=SEQ_MODE_POTEDIT;
      } else if(BUTTON_IS_PRESSED(BUTTON_L_SHIFT)) {
         gRunningState.currentMode=SEQ_MODE_LEFT;
      } else if(BUTTON_IS_PRESSED(BUTTON_R_SHIFT)) {
         gRunningState.currentMode=SEQ_MODE_RIGHT;
      }
      break;
   case SEQ_MODE_POTEDIT: // can shift to none when both pots are at 0
      if(POT_MAP(POT_1, SEQ_TRED_OFF, SEQ_TRED_TOTAL) == SEQ_TRED_OFF &&
         POT_MAP(POT_5, SEQ_GLBL_OFF, SEQ_GLBL_TOTAL) == SEQ_GLBL_OFF) {
         gRunningState.currentMode=SEQ_MODE_NONE;
      }   
      break;
   case SEQ_MODE_STEPEDIT: // can shift to none when either l-shift is JUST pressed or any green button
      // todo check this
      break;
   case SEQ_MODE_LEFT: // can shift to none when released
      if(!BUTTON_IS_PRESSED(BUTTON_L_SHIFT)) {
         gRunningState.currentMode=SEQ_MODE_NONE;
      }
      break;
   case SEQ_MODE_RIGHT: // can shift to none when released
      if(!BUTTON_IS_PRESSED(BUTTON_R_SHIFT)) {
         gRunningState.currentMode=SEQ_MODE_NONE;
      }
      break;   
   }
}

// handles mode NONE. When we are not in a specific mode
// need to respond to buttons which turn on and off notes
// and also respond to F buttons which switch tracks
static void 
uiHandleMainMode()
{
   uint8_t i, len;
   
   len=gSeqState.tracks[gRunningState.currentTrack].numSteps;
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
   for (i = BUTTON_FUNCTION; i < BUTTON_FUNCTION + 8; i++) {
      if (BUTTON_JUST_PRESSED(i)) {
         // if the track is not current, switch to it
         gRunningState.currentTrack = i;
      }
   }
   
}

// if in pot mode, we will respond to pot 1, 2, 5 and 6
// if 1 or 5 are set to non-zero, we set a new submode
// if 2 or 6 is moved, we check submode and alter setting based on it
// some of the modes are y/n which means that a button press toggles it
static void
uiHandlePotMode(void)
{
   uint8_t pv;
   uint8_t global = 0;

   // set the correct sub-mode based on pots 1 and 5

   // see if we are in global edit
   pv = POT_MAP(POT_5, SEQ_GLBL_OFF, SEQ_GLBL_TOTAL);
   if (pv == SEQ_GLBL_OFF) {
      // see if we are in track edit
      pv = POT_MAP(POT_1, SEQ_TRED_OFF, SEQ_TRED_TOTAL);      
   }
   else       
      global = 1;
      
   if (!pv)
      return; // this shouldn't happen - when both are 0

   // set this up so that led's can check it, etc
   gRunningState.currentSubMode = pv;
   if(global)
      gRunningState.currentSubMode |= SEQ_GLBL_OR_TRED_SELECTOR;


   if (!global && POT_JUST_CHANGED(POT_2)) { 
      // if one of the track edit modes is set apply it

   }
   else if (global && POT_JUST_CHANGED(POT_6)) { 
      // if one of the global edit modes, apply it

   }
   else if (BUTTON_JUST_PRESSED(BUTTON_TOGGLE_SETTING)) {
      // if in a mode that has a toggle setting, toggle it
      switch (pv) {

      }
   }

}

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
      break;
   case SEQ_MODE_LEFT:
      break;
   case SEQ_MODE_RIGHT:
      break;   
   }
   
   
  
}


