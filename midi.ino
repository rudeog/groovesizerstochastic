// midiA.h library info http://playground.arduino.cc/Main/MIDILibrary
// Reference http://arduinomidilib.fortyseveneffects.com/

byte lastNote = 0;

void HandleNoteOn(byte channel, byte pitch, byte velocity) 
{ 
  // Do whatever you want when you receive a Note On.
  // Try to keep your callbacks short (no delays ect) as the contrary would slow down the loop()
  // and have a bad impact on real-time performance.
  if (channel == 10 && pitch < 112) // receive patch changes on triggerChannel (default channel 10); there are 112 memory locations
  {      
    if (checkToc(pitch))
    {
      recall = true; // we're ready to recall a preset
      pageNum = pitch * 4; // one memory location is 4 pages long
      cued = pitch; // the pattern that will play next is the value of "pitch" - need this to blink lights
      head = pitch; // need this for pattern chaining to work properly
      mode = 4; // make sure we're in trigger mode, if we weren't already
      trigPage = pitch/32; // switch to the correct trigger page
      controlLEDrow = B00000001; // light the LED for trigger mode
      controlLEDrow = bitSet(controlLEDrow, trigPage + 1); // light the LED for the page we're on     
    }
  }
  else if (mode != 7  && pitch > 0 && channel == 10)
  {
    if (!seqRunning && !midiClock)
    { 
      if (velocity != 0) // some instruments send note off as note on with 0 velocity
      {
        lastNote = pitch;
        int newVelocity = (pot[2] + velocity < 1023) ? pot[2] + velocity : 1023; 
      }
      else // velocity = 0;
      {
      }
    }
  }
}

void HandleNoteOff(byte channel, byte pitch, byte velocity) 
{
} 

/* Called 24 times per QN when we are receiving midi clock
 *  
 */
void HandleClock(void)
{
  lastClock = millis();
  
  if (syncStarted)
  {
    seqRunning = false;

    if (clockCounterSlave >= nextStepIncomingPulse)
    {
      playStep();
      scheduleNextStepSlave();
    }

    // see if it's time to play a flam and handle the next one
    for (byte i = 0; i < 12; i++) // for each of the twelve tracks
    {
      if (track[i].nextFlamPulse != 0 && track[i].nextFlamPulse == clockCounterSlave)
      {
        handleFlam(i);
      }
    }
    clockCounterSlave++;
  }
}

/* Called when we receive a MIDI start
 *  
 */
void HandleStart (void)
{
    syncStarted = true;
    seqReset();
}

/* Called when we receive a midi stop
 *  
 */
void HandleStop (void)
{
  syncStarted = false;
  seqReset();
  seqRunning = false;
  ledsOff();
  updateLeds();
}





