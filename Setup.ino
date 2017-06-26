// *************************************
//           THE SETUP
// *************************************

void setup() {
  // setup for the sequencer timer 
  Timer1.initialize(5208); // default 120bpm @ 96ppq (1 quarter = 500ms, so 500000/96) 
  Timer1.attachInterrupt(internalClock);

  //define pin modes 
  pinMode(LEDlatchPin, OUTPUT);
  pinMode(LEDclockPin, OUTPUT); 
  pinMode(LEDdataPin, OUTPUT);

  pinMode(BUTTONlatchPin, OUTPUT);
  pinMode(BUTTONclockPin, OUTPUT); 
  pinMode(BUTTONdataPin, INPUT);

  for(byte bit = 0; bit < 3; bit++)
    pinMode(select[bit], OUTPUT);  // set the three 4051 select pins to output

  clearAll(0); // clear everything at the start, just to be on the safe side

  seqTrueStep = 0;
  seqCurrentStep = 0;

  // initialize the track variables
  for (byte i = 0; i < 12; i++)
  {
    track[i].midiChannel = 10;
    track[i].midiNoteNumber = 36 + i;
    track[i].level = 85;
    track[i].accentLevel = 127;
    track[i].flamDelay = 16;
    track[i].flamDecay = 20;
  }

  // Initiate MIDI communications, listen to all channels
  midiA.begin(MIDI_CHANNEL_OMNI);    

  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  midiA.setHandleNoteOn(HandleNoteOn);  // only put the name of the function here - functions defined in HandleMidi

  midiA.setHandleNoteOff(HandleNoteOff);

  midiA.setHandleClock(HandleClock);

  midiA.setHandleStart(HandleStart);

  midiA.setHandleStop(HandleStop); 

  //begin I2C Bus
  Wire.begin();

  //begin EEPROM with I2C Address 
  mem_1.begin(0,0);//addr 0 (DEC) type 0 (defined as 24LC256)

  // check if character definitions exists in EEPROM by seeing if the first 10 bytes of the expected character array add up to 93
  mem_1.readPage(508, rwBuffer); // 0 is the first page of the EEPROM (511 is last) - a page is 64 bytes long
  byte x = 0;
  for (byte i = 0; i < 10; i++)
  {
    x = x + rwBuffer[i];
  }
  if (x != 93) // yikes, there's a problem - the EEPROM isn't set up properly
  {
    //give us a bad sign!
    LEDrow[0] =  B10101010;
    LEDrow[1] =  B01010101;
    LEDrow[2] =  B10101010;
    LEDrow[3] =  B01010101;
    LEDrow[4] =  B10101010;
    for (byte i = 0; i < 100; i++)
    { 
      updateLeds();
      delay(5);
    }  
  }

  else
  {
    //give us a good sign!
    for (byte i = 0; i < 5; i++)
      LEDrow[i] =  B11111111;

    for (byte i = 0; i < 100; i++)
    { 
      updateLeds();
      delay(5);
    }
  }

  //set the pot locks
  for (byte i = 0; i < 20; i++) // do this a bunch of times for the average values to settle
    getPots();
  lockPot(6); // use value of 6 to lock all pots

  tocRead(); // update the toc array with a read from EEPROM page 448 - defined in HelperFunctions
  // also loads preferences

  triggerChannel = (triggerChannel > 16) ? 16 : triggerChannel; // in case the prefs haven't been saved yet and we have a weird value here
  thruOn = (thruOn > 2) ? 1 : thruOn; // again, in case prefs haven't been saved yet
  // midiSyncOut = true; // the MPX8 doesn't like sync out ;^(          

  // message on boot
  for (int i = 0; i < 1000; i++)
  {
    number = versionNumber; // the version number
    showNumber();
    updateLeds();
  }

  // are we waiting for a MIDI start message?
  check_switches();
  if (pressed[32])
  {
    ledsOff();
    updateLeds();
  }
}

