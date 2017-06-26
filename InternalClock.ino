void internalClock()
{
  if (clockCounter % 4 == 0)
  {
    if (midiSyncOut && !midiClock)            
      midiA.sendRealTime(midi::Clock); // send a midi clock pulse

    if (startSeq)
    {
      startSeq = false;
      seqRunning = true;
      clockCounter = 0;
    }
  }

  if (seqRunning)
  {
    if (clockCounter >= nextStepPulse)
    {
      playStep();
      scheduleNextStep();
    }

    // see if it's time to play a flam and handle the next one
    for (byte i = 0; i < 12; i++) // for each of the twelve tracks
    {
      if (track[i].nextFlamPulse != 0 && track[i].nextFlamPulse == clockCounter)
      {
        handleFlam(i);
      }
    }
  }

  clockCounter++;
}





