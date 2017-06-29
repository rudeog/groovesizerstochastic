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
    memset(gRunningState.trackPositions,0,NUM_TRACKS);
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

