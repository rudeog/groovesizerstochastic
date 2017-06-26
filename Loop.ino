// *************************************
//           THE LOOP
// *************************************

// welcome to the loop
void loop() {
  // Call midiA.read the fastest you can for real-time performance.
  midiA.read();
  // there is no need to check if there are messages incoming if they are bound to a Callback function. 

  midiClock = (millis() - lastClock < 150) ? true : false; // are we currently receiving MIDI clock
  clockStarted = (syncStarted && midiClock) ? true : false; // both need to be true for clockStarted to be true

    boolean tempoDelayChange = false;

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
    userControl();
    LEDs();
}





