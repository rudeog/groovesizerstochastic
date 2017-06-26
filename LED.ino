void LEDs()
{
 // blink the led for each step in the sequence
static byte stepLEDrow[4];
// controlLEDrow is defined as global variable above so we can set it from helper functions
static unsigned long ledBlink; // for blinking an LED
static unsigned long ledBlink2; // for blinking another LED 
  // ******************************************
  //     LED setup: work out what is lit
  //     end of button check if/else structure
  // ******************************************
  if (showingNumbers)
  {
    showNumber();
  }
  else if (showingWindow)
  {
    for (byte i = seqFirstStep; i < (seqFirstStep + seqLength); i++)
      bitSet(LEDrow[i / 8], i % 8);
  }
  else
  {
    if (mode < 3) // editing tracks 0 - 11
    {
      // we always start with steps that are On and then work out what to blink or flash from there
      for (byte i = 0; i < 32; i++)
      {
        if (checkStepOn(currentTrack, i))
          bitSet(LEDrow[i / 8], i % 8); 
      }
    }

    if (mode == 3)
    {
      for (byte i = 0; i < 12; i++) // show all the active steps (on all tracks) on one grid
      {
        for (byte j = 0; j < 4; j++)
          LEDrow[j] |= track[i].stepOn[j]; 
      }
    }

    if (mode < 4 && mode > 0) // blink selected steps if we're not in trigger mode
    {
      // blink step selected leds
      if (millis() > ledBlink + 100)
      {
        for (byte i = 0; i < 4; i++)
          LEDrow[i] ^= *edit[i];

        if (millis() > ledBlink + 200)
          ledBlink = millis(); 
      } 
    }

    if (mode == 4) // we're in trigger mode
    {
      for (byte i = 0; i < 4; i++)
        LEDrow[i] = toc[trigPage*4 + i];

      // slow blink nowPlaying
      if (nowPlaying < 255 && nowPlaying / 32 == trigPage) // we use 255 to turn nowPlaying "off", and only show if we're on the appropriate trigger page
      {
        if (millis() > ledBlink + 200)
        {
          LEDrow[(nowPlaying % 32) / 8] ^= 1<<(nowPlaying%8);

          if (millis() > ledBlink + 400)
            ledBlink = millis(); 
        }
      } 

      // (fast) blink the pattern that's cued to play next
      if (cued < 255 && cued / 32 == trigPage) // we use 255 turn cued "off", and only show if we're on the appropriate trigger page
      {
        if (millis() > ledBlink2 + 100)
        {
          LEDrow[(cued % 32) / 8] ^= 1<<(cued%8);

          if (millis() > ledBlink2 + 200)
            ledBlink2 = millis(); 
        }
      }

      // (fast) blink the location that needs to be confirmed to overwrite
      else if (confirm < 255 && confirm / 32 == trigPage)
      {
        if (millis() > ledBlink2 + 100)
        {
          LEDrow[(confirm % 32) / 8] ^= 1<<(confirm%8);

          if (millis() > ledBlink2 + 200)
            ledBlink2 = millis(); 
        }
      }

      bitClear(controlLEDrow, 6);
      bitClear(controlLEDrow, 5);
    }

    // this blinks the led for the current step
    if (seqRunning || syncStarted)
    {
      for (byte i = 0; i < 4; i++)
      {
        stepLEDrow[i] = (lastStep / 8 == i) ? B00000001 << (lastStep % 8) : B00000000;
        LEDrow[i] ^= stepLEDrow[i];
      }
    }

    // update the controlLEDrow
    switch (mode)
    {
    case 0:
      controlLEDrow = B00000010;
      controlLEDrow = controlLEDrow << currentTrack % 6;
      if (currentTrack > 5)
        bitSet(controlLEDrow, 7);
      break;
    case 3:
      controlLEDrow = B10000001; // master page
      if (seqRunning || syncStarted)
        bitSet(controlLEDrow, 1);
      if (mutePage)
        bitSet(controlLEDrow, 3);
      if (skipPage)
        bitSet(controlLEDrow, 4);        
      break;
    case 4:
      controlLEDrow = B00000001; // trigger page
      controlLEDrow = bitSet(controlLEDrow, trigPage + 1); // light the LED for the page we're on
      if (followAction == 1 || followAction == 2)  // light the LED for followAction 1 or 2
          controlLEDrow = bitSet(controlLEDrow, 7 - followAction);
      break;
    case 5:
      controlLEDrow = 0; // clear it
      // set according to the preferences
      if (midiSyncOut)
        bitSet(controlLEDrow, 1);
      if (thruOn)
        bitSet(controlLEDrow, 2);
      if (midiTrigger)
        bitSet(controlLEDrow, 3);
      break;
    }

    LEDrow[4] = controlLEDrow; // light the LED for the mode we're in
  }
  // this function shifts out the the 5 bytes corresponding to the led rows
  // declared in ShiftOut (see above)
  updateLeds();
}

// this function shifts out the the 4 bytes corresponding to the led rows
void updateLeds(void)
{
  static boolean lastSentTop = false; // we want to alternate sending the top 2 and bottom 3 rows to prevent an edge case where 4 rows of LEDs lit at the same time sourcing too much current
  //ground LEDlatchPin and hold low for as long as you are transmitting
  digitalWrite(LEDlatchPin, 0);
  if (!lastSentTop) // send the top to rows
  {
    shiftOut(LEDdataPin, LEDclockPin, B00000000);
    shiftOut(LEDdataPin, LEDclockPin, B00000000); 
    shiftOut(LEDdataPin, LEDclockPin, B00000000);
    shiftOut(LEDdataPin, LEDclockPin, LEDrow[1]); 
    shiftOut(LEDdataPin, LEDclockPin, LEDrow[0]);
    lastSentTop = true;
  }
  else // ie. lastSentTop is true, then send the bottom 3 rows
  {
    shiftOut(LEDdataPin, LEDclockPin, LEDrow[4]);
    shiftOut(LEDdataPin, LEDclockPin, LEDrow[3]); 
    shiftOut(LEDdataPin, LEDclockPin, LEDrow[2]);
    shiftOut(LEDdataPin, LEDclockPin, B00000000); 
    shiftOut(LEDdataPin, LEDclockPin, B00000000);
    lastSentTop = false;
  }
  //return the latch pin high to signal chip that it 
  //no longer needs to listen for information
  digitalWrite(LEDlatchPin, 1);
}

