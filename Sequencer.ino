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

void scheduleNextStep() // as internal clock master
{
  if (seqTrueStep%2 != 0) // it's an uneven step, ie. the 2nd shorter 16th of the swing-pair
    swing16thDur = (2*clockDiv[clockDivSelect]) - swing16thDur;
  else // it's an even step ie. the first longer 16th of the swing-pair
  {
    swing16thDur = map(swing, 0, 255, clockDiv[clockDivSelect], ((2*clockDiv[clockDivSelect])/3)*2);
  }
  seqTrueStep = (seqTrueStep + 1) % 32; 
  
  nextStepPulse = clockCounter + swing16thDur; // make sure we stay aligned with the MIDI clock pulses we send out
  if (seqCurrentStep == 0 || seqCurrentStep == 16 || seqCurrentStep == seqFirstStep)
  {
    while (nextStepPulse % 4 != 0)
      nextStepPulse--;
  } 
}

void handleFlam(byte trNum)
{
  // send the flam note
  midiA.sendNoteOff(track[trNum].midiNoteNumber, 127, track[trNum].midiChannel);
  midiA.sendNoteOn(track[trNum].midiNoteNumber, track[trNum].nextFlamLevel, track[trNum].midiChannel);

  // schedule the next flam
  if (syncStarted)
    scheduleFlamSlave(trNum, track[trNum].nextFlamLevel);
  else
    scheduleFlam(trNum, track[trNum].nextFlamLevel);
}

void scheduleNextStepSlave() // as MIDI clock slave
{
  if (seqTrueStep%2 != 0) // it's an uneven step, ie. the 2nd shorter 16th of the swing-pair
    incomingSwingDur = 2 * clockDivSlave[clockDivSlaveSelect] - incomingSwingDur;
  else // it's an even step ie. the first longer 16th of the swing-pair
  {
    incomingSwingDur = map(swing, 0, 255, clockDivSlave[clockDivSlaveSelect], ((2*clockDivSlave[clockDivSlaveSelect])/3)*2);
  }
  seqTrueStep = (seqTrueStep + 1) % 32;
  nextStepIncomingPulse = nextStepIncomingPulse + incomingSwingDur; 
}

void scheduleFlam(byte Track, byte Velocity)
{
  if (fxFlam[Track] != 0)
  {
    if (Velocity > fxFlamDecay)
    {
      track[Track].nextFlamLevel = Velocity - fxFlamDecay;
      track[Track].nextFlamPulse = clockCounter + fxFlamDelay;
      fxFlam[Track] = 2;
    }
    else
    {
      track[Track].nextFlamPulse = 0;
      for (byte i = 0; i < 12; i++)
        fxFlam[i] = 0; 
    }
  }
  else if (fxFlam[Track] == 0)
  {
    if (Velocity > track[Track].flamDecay)
    {
      track[Track].nextFlamLevel = Velocity - track[Track].flamDecay;
      track[Track].nextFlamPulse = clockCounter + track[Track].flamDelay;
    }
    else
      track[Track].nextFlamPulse = 0;
  }
}

void scheduleFlamSlave(byte Track, byte Velocity)
{
  byte slaveDelay = map(track[Track].flamDelay, 4, 48, 1, 24);
  if (fxFlam[Track] != 0)
  {
    if (Velocity > fxFlamDecay)
    {
      track[Track].nextFlamLevel = Velocity - fxFlamDecay;
      track[Track].nextFlamPulse = clockCounterSlave + fxFlamDelay;
      fxFlam[Track] = 2;
    }
    else
    {
      track[Track].nextFlamPulse = 0;
      for (byte i = 0; i < 12; i++)
        fxFlam[i] = 0; 
    }
  }
  else if (fxFlam[Track] == 0)
  {
    if (Velocity > track[Track].flamDecay)
    {
      track[Track].nextFlamLevel = Velocity - track[Track].flamDecay;
      track[Track].nextFlamPulse = clockCounterSlave + slaveDelay;
    }
    else
      track[Track].nextFlamPulse = 0;
  }
}





