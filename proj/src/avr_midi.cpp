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
static void handleNoteOn(byte channel, byte pitch, byte velocity) 
{   
}

static void handleNoteOff(byte channel, byte pitch, byte velocity) 
{
} 

static uint8_t gJustStarted=0;
static uint16_t gStartTime=0;
static uint8_t gCounter=0;
// Called 24 times per QN when we are receiving midi clock
static void handleClock(void)
{  
   if(SLAVE_MODE()) { // slave mode   
      seqClockTick();
      
      // to calculate bpm
      if(gCounter == 0) {
         if(!gJustStarted)
            gRunningState.tempo = (uint8_t) (int32_t)(60000 / ((int16_t)((uint16_t)millis() - gStartTime)));
         gStartTime=(uint16_t)millis();      
      }
      gCounter++;
      if(gCounter > 23)
          gCounter=0;
      gJustStarted=0;
  }   
}

/* Called when we receive a MIDI start
 *  
 */
static void handleStart (void)
{  
  if(SLAVE_MODE()) { // slave mode
    seqSetTransportState(TRANSPORT_STARTING);
    gJustStarted=1; // see handleClock
  }
}

/* Called when we receive a midi stop
 *  
 */
void handleStop (void)
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
  midiA.setHandleNoteOn(handleNoteOn);  // only put the name of the function here - functions defined in HandleMidi
  midiA.setHandleNoteOff(handleNoteOff);
  midiA.setHandleClock(handleClock);
  midiA.setHandleStart(handleStart);
  midiA.setHandleStop(handleStop); 

  // setup based on current sequencer state
  midiSetThru();
}
