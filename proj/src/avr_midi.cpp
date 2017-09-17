#include "avr_main.h"
// midiA.h library info http://playground.arduino.cc/Main/MIDILibrary
// Reference http://arduinomidilib.fortyseveneffects.com/

// creates a global var that represents midi
// (size of midi is 183)
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, midiA);


//
// PUBLIC
//
struct Playing {
   uint16_t time;   
   uint8_t noteNum;
   uint8_t isOn : 1;
   uint8_t chan : 4;
};

static Playing gPlayingNotes[NUM_TRACKS];

// length is in ms
void
midiPlayNote(uint8_t chan, uint8_t note, uint8_t vel, uint16_t length, uint8_t track)
{
   if(gPlayingNotes[track].isOn) {
      // turn it off
      midiA.sendNoteOff(gPlayingNotes[track].noteNum,0,gPlayingNotes[track].chan);      
   }
   
   // signify on
   gPlayingNotes[track].time=millis()+length;
   gPlayingNotes[track].isOn=1;
   gPlayingNotes[track].chan=chan;
   gPlayingNotes[track].noteNum=note;
   
   // chan here is 1 based   
   midiA.sendNoteOn(note, vel, chan);
}

// called from the main loop.
// read midi port, issue callbacks
void midiUpdate(void)
{
  midiA.read();
  
  // see if any notes need to go off
  for(uint8_t track=0;track<NUM_TRACKS;track++) {
     if(gPlayingNotes[track].isOn) {
        if((int16_t)((uint16_t)millis()-gPlayingNotes[track].time) >= 0) {
           midiA.sendNoteOff(gPlayingNotes[track].noteNum,0,gPlayingNotes[track].chan);      
           gPlayingNotes[track].isOn=0;
        }
     }
  }  
}

void midiSendClock(void)
{
  midiA.sendRealTime(midi::Clock);
}

void midiSendStart(void)
{
  midiA.sendRealTime(midi::Start);
}

void midiSendStop(void)
{
  midiA.sendRealTime(midi::Stop); // send a midi clock stop signal)  
}
static void HandleNoteOn(byte channel, byte pitch, byte velocity) 
{ 
  #if 0
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
  #endif
}


static void HandleNoteOff(byte channel, byte pitch, byte velocity) 
{
} 

static uint8_t gJustStarted=0;
// Called 24 times per QN when we are receiving midi clock
static void HandleClock(void)
{  
  if(SLAVE_MODE()) { // slave mode
   if(gJustStarted)
      gJustStarted=0; // throw away first clock tick, see seqSetTransportState for more about this nonsense
   else {
      seqClockTick();   
      //gRunningState.tempo
   }
  }   
}

/* Called when we receive a MIDI start
 *  
 */
static void HandleStart (void)
{  
  if(SLAVE_MODE()) { // slave mode
    seqSetTransportState(TRANSPORT_STARTED);
    gJustStarted=1;
  }
}

/* Called when we receive a midi stop
 *  
 */
void HandleStop (void)
{
  if(SLAVE_MODE()) { // slave mode
    seqSetTransportState(TRANSPORT_STOPPED);
  }  
}

void midiSetThru(void)
{
  if(gSeqState.midiThru) {
    midiA.turnThruOn(midi::Full);
  } else {
    midiA.turnThruOff();
  }  
}

void midiSetup(void)
{
  // Initiate MIDI communications, listen to all channels
  midiA.begin(MIDI_CHANNEL_OMNI);    

  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  midiA.setHandleNoteOn(HandleNoteOn);  // only put the name of the function here - functions defined in HandleMidi
  midiA.setHandleNoteOff(HandleNoteOff);
  midiA.setHandleClock(HandleClock);
  midiA.setHandleStart(HandleStart);
  midiA.setHandleStop(HandleStop); 

  // setup based on current sequencer state
  midiSetThru();
}
