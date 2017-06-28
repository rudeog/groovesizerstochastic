/* This gets called when timer1 ticks. This corresponds to BPM so it
 *  will get called 96 times per quarter note (our resolution)
 */
void internalClock()
{
  /* Midi clock is sent 24 times per quarter note, so divide 
   */
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
      playStep(); // in Sequencer
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



// things that need to happen as a consequence of the bpm changing
void setBPM(uint8_t BPM)
{
  gSeqState.tempo=BPM;
  if(BPM==0) { // bpm of 0 indicates ext clock sync
    gRunningState.tempo=DEFAULT_BPM; // until we get clock sync we can't know what the tempo is    
    Timer1.detachInterrupt();
    // TODO start receiving midi clock
  } else {
    gRunningState.tempo=BPM;
    // use the same tick interval as midi clock to simplify matters    
    Timer1.setPeriod(1000000/((BPM/60)*24)); // with period in microseconds (same as 1000000*60/BPM/24)
    Timer1.attachInterrupt(internalClock);
  }
  
