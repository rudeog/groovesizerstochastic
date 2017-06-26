/* Main loop
 *  
 */

// welcome to the loop
void loop() 
{
  // force inclusion
  //gPatterns[0].tracks[0].steps[0].velocity=10;
  
  // Call midiA.read the fastest you can for real-time performance.
  // This will invoke callbacks - see HandleMidi   
  midiA.read();
  
  // are we currently receiving MIDI clock
  // millis returns number of milliseconds since beginning of time  
  // If the last clock we receieved was more that 150ms ago we can be fairly sure
  // that we are not receiving midi clock's anymore. At 17bpm we'd be receiving a clock tick
  // every 147 milliseconds (we receive 24 per beat)
  if(millis() - lastClock < 150)
    midiClock = true;
  else
    midiClock = false; 

  // both need to be true for clockStarted to be true
  if(syncStarted && midiClock)
    clockStarted = true;
  else
    clockStarted = false; 


  if (goLoad)
  {
    goLoad = false;
    loadPatch(cued);
  }

  // we check buttons at the top of the loop, the "just" flags get reset here every time
  // it's best to reset the "just" flags manually whenever we check for "just" and find it true - use "clearJust()"
  // it prevents another just function from being true further down in the same loop
  check_switches(); //defined in ButtonCheck in the tab above

  // get the pot values
  getPots(); // defined in HelperFunctions

    // see if it's time to lock the pots
  if (lockTimer != 0 && millis() - lockTimer > 1000)
  {
    lockPot(6); // value of 6 locks all pots
  }

  ledsOff();
  buttonFunction(); // the functions of the buttons depend on the mode 
  
  // this is the main function that checks pots etc.
  userControl();
  LEDs();
}





