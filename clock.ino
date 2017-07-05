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


