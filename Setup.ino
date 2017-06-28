// *************************************
//           THE SETUP
// *************************************

void setup() {
  // setup for the sequencer timer 
  Timer1.initialize(0); 
  
  //define pin modes 
  pinMode(LEDlatchPin, OUTPUT);
  pinMode(LEDclockPin, OUTPUT); 
  pinMode(LEDdataPin, OUTPUT);

  pinMode(BUTTONlatchPin, OUTPUT);
  pinMode(BUTTONclockPin, OUTPUT); 
  pinMode(BUTTONdataPin, INPUT);

  //TODO select is a bit generic of a term
  for(byte bit = 0; bit < 3; bit++)
    pinMode(select[bit], OUTPUT);  // set the three 4051 select pins to output

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

  // lock current pot values  
  potsUpdate();
  lockPot(6); // use value of 6 to lock all pots

  // message on boot
  for (int i = 0; i < 1000; i++)
  {
    number = gVersionNumber; // the version number
    showNumber();
    updateLeds();
  }

  // are we waiting for a MIDI start message?
  check_switches();
  if (BUTTON_IS_PRESSED(32))
  {
    ledsOff();
    updateLeds();
  }
}

