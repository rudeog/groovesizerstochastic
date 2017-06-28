// *************************************
//           HELPER FUNCTIONS
// *************************************

// read the table of contents (TOC) from the EEPROM (page 448)
// also recalls preferences
void tocRead()
{
  mem_1.readPage(448, rwBuffer);
  for (byte i = 0; i < 14; i++)
  {
    toc[i] = rwBuffer[i];
  }

  // update the preferences
  midiSyncOut = rwBuffer[14]; // send out 24ppq clock pulses to slave MIDI devices to this Groovesizer
  thruOn = rwBuffer[15]; // echo MIDI data received at input at output
  checkThru();
  midiTrigger = rwBuffer[16]; // send pattern trigger notes to change patterns on slaved Groovesizers
  triggerChannel = rwBuffer[17]; // the MIDI channel that pattern change messages are sent and received on
}

void assignPreferences()
{
  rwBuffer[14] = midiSyncOut; // send out 24ppq clock pulses to slave MIDI devices to this Groovesizer
  rwBuffer[15] = thruOn; // echo MIDI data received at input at output
  rwBuffer[16] = midiTrigger; // send pattern trigger notes to change patterns on slaved Groovesizers
  rwBuffer[17] = triggerChannel; // the MIDI channel that pattern change messages are sent and received on
}

void tocWrite(byte addLoc)
{
  byte tocByte = addLoc/8; // in which byte of the toc array is the location (each byte corresponds to a row of buttons)
  toc[tocByte] = bitSet(toc[tocByte], addLoc%8); // turns the appropriate byte on (1)
  for (byte i = 0; i < 14; i++) // update the read/write buffer with the current toc
    rwBuffer[i] = toc[i];

  // remember to save preferences too, since they're on the same EEPROM page
  assignPreferences();

  mem_1.writePage(448, rwBuffer); // write the buffer to the toc page on the EEPROM (448)
}

void tocClear(byte clearLoc)
{
  byte tocByte = clearLoc/8; // in which byte of the toc array is the location (each byte corresponds to a row of buttons)
  toc[tocByte] = bitClear(toc[tocByte], clearLoc%8); // turns the appropriate byte off (0)
  for (byte i = 0; i < 14; i++) // update the read/write buffer with the current toc
    rwBuffer[i] = toc[i];

  // remember to save preferences too, since they're on the same EEPROM page
  assignPreferences();

  mem_1.writePage(448, rwBuffer); // write it to the toc page on the EEPROM (448)

}

boolean checkToc(byte thisLoc) // check whether a memory location is already used or not
{
  byte tocByte = thisLoc/8; // in which byte of the toc array is the location (each byte corresponds to a row of buttons)
  if bitRead(toc[tocByte], thisLoc%8)
    return true;
  else
    return false;  
}

void savePrefs()
{
  //preferences are saved on the same EEPROM page (448) as the toc, so make sure to resave the toc when saving preferences
  for (byte i = 0; i < 14; i++) // update the read/write buffer with the current toc
    rwBuffer[i] = toc[i];

  assignPreferences(); // defined above

  mem_1.writePage(448, rwBuffer); // write the buffer to the toc page on the EEPROM (448) 
}

void savePatch()
{
  //transposeFactor = 0; // reset the transpose factor to 0
  static boolean pageSaved = false;
  static byte progress; // we want to stagger the save process over consecutive loops, in case the process takes too long and causes an audible delay
  if (!pageSaved)
  {    
    BUTTON_CLEAR_JUST_INDICATORS();
    longPress = 0;
    //mem_1.readPage(pageNum, rwBuffer);
    pageSaved = true;
    progress = 0;
  }

  switch (progress) { // we want to break our save into 4 seperate writes on 4 consecutive passes through the loop
  case 0:
    // pack our 64 byte buffer with the goodies we want to send to the EEPROM

    for (byte i = 0; i < 3; i++) // for the first set of 3 tracks
    {
      packTrackBuffer(i);
    }  

    mem_1.writePage(pageNum, rwBuffer);
    progress = 1;
    break;

  case 1:

    for (byte i = 3; i < 6; i++) // for the second set of 3 tracks
    {
      packTrackBuffer(i);
    }  

    // add the division bytes
    rwBuffer[57] = clockDivSelect;
    rwBuffer[58] = clockDivSlaveSelect;

    mem_1.writePage(pageNum + 1, rwBuffer);
    progress = 2;
    break;

  case 2:
    for (byte i = 6; i < 9; i++) // for the third set of 3 tracks
    {
      packTrackBuffer(i);
    }  

    // add first 7 of 14 master page bytes
    rwBuffer[57] = seqLength;
    rwBuffer[58] = seqFirstStep;
    rwBuffer[59] = programChange;
    for (byte i = 0; i < 4; i++)
      rwBuffer[60 + i] = seqStepMute[i];

    mem_1.writePage(pageNum + 2, rwBuffer);
    progress = 3;
    break;

  case 3:
    for (byte i = 9; i < 12; i++) // for the fourth set of 3 tracks
    {
      packTrackBuffer(i);
    }

    // add second 7 of 14 master page bytes
    rwBuffer[57] = swing;
    rwBuffer[58] = followAction;
    rwBuffer[59] = bpm;
    for (byte i = 0; i < 4; i++)
      rwBuffer[60 + i] = seqStepSkip[i];  

    mem_1.writePage(pageNum + 3, rwBuffer);
    save = false;
    pageSaved = false;
    break;
  }
}

void packTrackBuffer(byte Track)
{
  byte offset = 19 * (Track % 3); // each track is 19 bytes long
  rwBuffer[0 + offset] = track[Track].level;       
  rwBuffer[1 + offset] = track[Track].accentLevel;
  rwBuffer[2 + offset] = highByte(track[Track].flamDelay); // flamDelay stored as 2 bytes - the int is split into a high and low byte 
  rwBuffer[3 + offset] = lowByte(track[Track].flamDelay);
  rwBuffer[4 + offset] = track[Track].flamDecay;
  rwBuffer[5 + offset] = track[Track].midiChannel;
  rwBuffer[6 + offset] = track[Track].midiNoteNumber;
  for (byte j = 0; j < 4; j++)
  {
    rwBuffer[7 + offset + j] = track[Track].stepOn[j];
    rwBuffer[11 + offset + j] = track[Track].stepAccent[j];
    rwBuffer[15 + offset + j] = track[Track].stepFlam[j];
  } 
}

void unPackTrackBuffer(byte Track)
{
  byte offset = 19 * (Track % 3); // each track is 19 bytes long
  track[Track].level = rwBuffer[0 + offset];       
  track[Track].accentLevel = rwBuffer[1 + offset];
  track[Track].flamDelay = word(rwBuffer[2 + offset], rwBuffer[3 + offset]);
  track[Track].flamDecay = rwBuffer[4 + offset];
  track[Track].midiChannel = rwBuffer[5 + offset];
  track[Track].midiNoteNumber = rwBuffer[6 + offset];
  for (byte j = 0; j < 4; j++)
  {
    track[Track].stepOn[j] = rwBuffer[7 + offset + j];
    track[Track].stepAccent[j] = rwBuffer[11 + offset + j];
    track[Track].stepFlam[j] = rwBuffer[15 + offset + j];
  } 
}

void loadPatch(byte patchNumber) // load the specified location number
{
  saved = true; // needed for follow actions 1 and 2
  recall = false;
  pageNum = patchNumber * 4; // each location is 4 pages long
  clearSelected();
  BUTTON_CLEAR_JUST_INDICATORS();
  cued = patchNumber; // for the duration of the load process, we set cued = patchNumber so we can loadContinue with loadPatch(cued)

  mem_1.readPage(pageNum, rwBuffer);

  // unpack our 64 byte buffer with the goodies from the EEPROM

  for (byte i = 0; i < 3; i++) // for the first set of 3 tracks
  {
    unPackTrackBuffer(i);
  }  

  mem_1.readPage(pageNum + 1, rwBuffer);

  for (byte i = 3; i < 6; i++) // for the second set of 3 tracks
  {
    unPackTrackBuffer(i);
  }  

  // unpack the division bytes
  clockDivSelect = rwBuffer[57];
  clockDivSlaveSelect = rwBuffer[58];

  mem_1.readPage(pageNum + 2, rwBuffer);

  for (byte i = 6; i < 9; i++) // for the third set of 3 tracks
  {
    unPackTrackBuffer(i);
  }  

  // read first 7 of 14 master page bytes
  seqLength = rwBuffer[57];
  seqFirstStep = rwBuffer[58];
  seqLastStep = seqFirstStep + (seqLength - 1);
  // send a program change message if the patch is not the same as the current one
  if (programChange != rwBuffer[59])
  {
    programChange = rwBuffer[59];
    midiA.sendProgramChange(programChange, track[0].midiChannel);
  }
  for (byte i = 0; i < 4; i++)
    seqStepMute[i] = rwBuffer[60 + i];

  mem_1.readPage(pageNum + 3, rwBuffer);

  for (byte i = 9; i < 12; i++) // for the fourth set of 3 tracks
  {
    unPackTrackBuffer(i);
  }

  // read second 7 of 14 master page bytes
  swing = rwBuffer[57];
  followAction = rwBuffer[58];
  bpm = rwBuffer[59];
  bpmChange(bpm);
  for (byte i = 0; i < 4; i++)
    seqStepSkip[i] = rwBuffer[60 + i];  

  nowPlaying = cued; // the pattern playing is the one that was cued before
  cued = 255; // out of range since 112 patterns only - we'll use 255 to see if something is cue
}

void buttonCheckSelected()
{
  // check if a switch was just pressed and toggle the appropriate bit in the selected array
  for (byte i = 0; i < 32; i++)
  { 
    if (BUTTON_JUST_PRESSED(i))
    {   
      *edit[i/8] ^= (1<<i%8); // toggle the bit, ie. turn 1 to 0, and 0 to 1
      if (mode == 0 && !checkStepOn(currentTrack, i)) // if we've just turn off a step, clear the same step of flams and accents as well
      {
        bitClear(track[currentTrack].stepAccent[i/8], i % 8);
        bitClear(track[currentTrack].stepFlam[i/8], i % 8);
      }
      BUTTON_CLEAR_JUST_INDICATORS();
    }
  } 
}








