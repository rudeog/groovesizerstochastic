#include "sequencer.h"

// This is x-platform code

//
// Globals
//

// these two are accessed externally as well
SequencerState gSeqState;
RunningState gRunningState;
// used to cycle from 0..5 for clock ticks
// to ensure that next clock tick starts at step boundary
// when user just hit start
static uint8_t gStepCounter;

// Forward decl
static inline void patternChangeCheck();
static inline void resetTracks();
static void switchToPattern(uint8_t pattern);
static inline uint8_t determineNextPattern();
static inline void advancePosition(uint8_t tr);
static inline void scheduleNext(void);
static inline void processStep(uint8_t trNum, uint8_t pos);
static inline void playNext(void);



//
// Public functions
//

// Initialize the sequencer
// does not initialize tempo
void seqSetup(void)
{
   SeqTrack *track;
   // initial 0 state should be fine for most things
   gSeqState.randomRegen = 1;
   
   for (uint8_t j = 0; j<NUM_TRACKS; j++) {
      track = &gSeqState.tracks[j];
      track->numSteps = DEFAULT_NUM_STEPS - 1; // 0 based
      track->midiNote = DEFAULT_MIDI_NOTE;
   }
      
   switchToPattern(0);
}

// called from main loop to see if we need to alter sequencer state
void
seqCheckState(void)
{ 
   if (gRunningState.transportState == TRANSPORT_STARTED) {

      // do we need to schedule any steps?
      // we only do this when we've had a clock tick.
      // more than that would be a waste of cpu
      if (gRunningState.haveClockTick) {
         scheduleNext();
         gRunningState.haveClockTick = 0;
      }

      // do we need to play any scheduled steps?
      // do this more frequently because of swing and div times that
      // might require more granularity
      playNext();

      // do we need to change patterns?
      // needs to be called after the above
      patternChangeCheck();
   }
}

// Start or stop playback
void
seqSetTransportState(uint8_t state)
{
   if (state == TRANSPORT_STARTED) {
      // this means that when you hit start, you could have a miniscule delay until next timer click
      gStepCounter = 0;
      // initialize current pattern
      switchToPattern(NUM_PATTERNS);
   }
   
   if(gSeqState.midiSendClock) {
      switch(state) {
      case TRANSPORT_STARTED:
         midiSendStart();
         break;
      case TRANSPORT_STOPPED:
         midiSendStop();
         break;        
      }
   }
   gRunningState.transportState = state;
}

// this is called from midi handler as well
void 
seqClockTick(void)
{
   /* Midi clock is sent 24 times per quarter note.
   */
   if (gSeqState.midiSendClock &&
      gRunningState.transportState == TRANSPORT_STARTED) {
      midiSendClock();
   }
      
   gRunningState.haveClockTick = 1;

   // step occurs every 6 ticks (24/4 as there are 4 steps per beat)
   if (gStepCounter  == 0) {
      gRunningState.lastStepTime = millis();
      gRunningState.lastStepTimeTriggered = 1;
   }

   gStepCounter++;
   if (gStepCounter > 5)
      gStepCounter = 0;

}

// change the BPM of the sequencer
void
seqSetBPM(uint8_t BPM)
{
   // note that we keep two different tempos. One is the set tempo, the other is possibly the
   // actual calculated tempo if we are syncing
  gSeqState.tempo=BPM;
  
  if(BPM==0) { // bpm of 0 indicates ext clock sync    
    gRunningState.tempo=DEFAULT_BPM; // until we get clock sync we can't know what the tempo is        
  } else {
    gRunningState.tempo=BPM;    
  }
  
  // let the clock do it's thing as well
  clockSetBPM(BPM);  
}

// invoke whenever user switches pattern
void seqSetPattern(uint8_t pat)
{
   if (gRunningState.transportState == TRANSPORT_STARTED)
      gRunningState.nextPattern = pat;
   else // transport is stopped so do a full switch
      switchToPattern(pat);
}

//
// Private
//

// check to see if we need to change to a new pattern.
// if so, figure out what pattern we need
static inline void 
patternChangeCheck()
{
   uint8_t hasPlayed = gRunningState.patternHasPlayed;
   uint8_t switchTo = gRunningState.nextPattern; // will be NUM_PATTERNS if not active
   
   // reset this bit always
   gRunningState.patternHasPlayed = 0;

   if (gRunningState.patternHold) {
      // don't switch patterns at all, and don't increment the current cycle
      // (playStep is responsible for cycling back to beginning of current pattern)
      return;
   }

   if (hasPlayed) { // a cycle has played      
      if (switchTo == NUM_PATTERNS) { // no manual switch active     
         // increment the counter of how many times we've played it
         gRunningState.patternCurrentCycle++;
         // see if we need to switch
         if (gRunningState.patternCurrentCycle >
            gSeqState.patterns[gRunningState.pattern].numCycles) {
            // we need to switch. figure out which pattern to switch to.
            // will come back with NUM_PATTERNS if no switch needed
            switchTo = determineNextPattern();            
         }
      }         
   }
   else {  // not at end of cycle. 
      if (gSeqState.delayPatternSwitch) { // user only wants to switch at end of cycle
         switchTo = NUM_PATTERNS; // don't switch right now
      }
   }

   if (switchTo < NUM_PATTERNS)
      switchToPattern(switchTo);
}

// prepare tracks to be scheduled
static inline void 
resetTracks()
{
   memset(gRunningState.trackStates, 0, NUM_TRACKS * sizeof(TrackRunningState));
}

// switch to a new pattern. this also 
// resets each track on the new pattern to be ready for scheduling
static void 
switchToPattern(uint8_t pattern)
{
   if (pattern == NUM_PATTERNS) // don't switch, just initialize
      pattern = gRunningState.pattern;

   LOGMESSAGE(0,"[pattern switch] pattern %d", pattern + 1);
   gRunningState.pattern = pattern;
   gRunningState.patternCurrentCycle = 0;
   gRunningState.patternHasPlayed = 0;
   gRunningState.nextPattern = NUM_PATTERNS;
   resetTracks();
}

// randomly determine which pattern will play next
// returns NUM_PATTERNS if no switch will happen
static inline uint8_t 
determineNextPattern()
{
   // value is 0..15 where 0 is never
   uint8_t *probs = gSeqState.patterns[gRunningState.pattern].nextPatternProb;
   uint8_t tot=0, rnd;
   // add up all values
   tot += (probs[0] & 0x0F); // pat 0
   tot += (probs[0] >> 4);   // pat 1
   tot += (probs[1] & 0x0F); // pat 2
   tot += (probs[1] >> 4);   // pat 3

   if (!tot) { // all are 0, don't switch
      return NUM_PATTERNS;
   }

   // generate
   rnd = rndRandom(tot);

   // see where it lies
   tot = 0;
   tot += (probs[0] & 0x0F); // pat 0
   if (rnd < tot) return 0;
   tot += (probs[0] >> 4);   // pat 1
   if (rnd < tot) return 1;
   tot += (probs[1] & 0x0F); // pat 2
   if (rnd < tot) return 2;
   tot += (probs[1] >> 4);   // pat 3
   if (rnd < tot) return 3;

   LOGMESSAGE(0,"Sprunt!"); // should never ever see this
#ifdef _WIN32   
   _exit(0);
#endif

   return NUM_PATTERNS;
}

// move track position to next
// flagging "pattern has played" flag if need be
static inline void 
advancePosition(uint8_t tr)
{   
   TrackRunningState *trState = &gRunningState.trackStates[tr];
   SeqTrack *track = &gSeqState.tracks[tr];

   // if this was the last position, we need to check for pattern change
   // track 0 determines the length of a cycle
   if (tr == 0 && trState->position == track->numSteps)
      gRunningState.patternHasPlayed = 1;

   // prepare for next step
   trState->position++;
   if (trState->position > track->numSteps) // both are 0 based
      trState->position = 0; // cycle back to beginning
}

/*
 * Some old notes on schedule next (may be obsolete): 
 * This is called from main loop. lastStepTime will be set to the last time that
 * a 4ppq clock tick occurred (ie a step). Under normal circumstances, we will schedule the next play time
 * of the track for that time. However we might have a clock divider or a swing to take into account.
 * Clock divider:
 *   4x or 2x:
 *     These need to occur 4 times (or 2) for each step, so we will use divPosition to determine where we are
 *     example (divider is 4x):
 *     isScheduled is false, lastStepTimeTriggered is true, we set it to false in this fn
 *     lastStepTime is set to 10 track position is 0 (no steps have played). divPosition is 0
 *     we set scheduledStart to 10, increment divPosition to 1 and set isScheduled to true
 *     (another function picks up the scheduled time, plays the note and clears isScheduled, sets trackposition to 1)
 *     lastStepTime is still 10, track position is 1 (1 step has played). divPosition is 1
 *     we set scheduledStart to 10+((divPosition) * (length of step/divider) ), increment divPosition to 2 (divider is 4)
 *     when divPosition would overflow to 0 we don't want to schedule anything else until we get a new scheduledStart time divComplete is set true
 *     lastStepTime is 10, isSched is false, lsttrig is false, if divComplete is true, do nothing
 *     lastStepTime is 20 (next step), track position should be 4, divPosition should be 0 (but might be other if tempo sped up)
 *     if divposition is nonzero, increment position by (4-divposition) and set it to 0
 *     set schedStart to 20, increment divPosition to 1 and isSChed to true
 *     if divComplete flag was set, clear it
 *     
 *     when lastStepTimeTriggered is true, we need to assume that we are on a whole step boundary. This becomes important
 *     if the tempo is changing in real time. For example, if tempo was 80, clock div is 4, and tempo changes to 160 and we've
 *     played two steps, we don't want to be two steps off from the real beat (hard to explain, draw it)
 *     - if divPosition is non-zero we will skip 4-divPosition steps to ensure that steps line up properly
 *     
 *     example (divider is 1/4x) 4x slower than tempo:
 *     isScheduled is false, lastStepTrig is true, we set it to false
 *     lastSteptime is 10 position 0, divPosition 0
 *     set schedstart to 10, increment divPosition to 1 and set isSched to true
 *     (another fn plays it, sets tp to 1...)
 *     another call, lastStepTimeTriggered is false, isSched is now false, we see that divPosition is non-zero so we do nothing 
 *     another call, lastStepTimeTriggered is true, but since divPosition is non-zero, we just increment it and do nothing else 
 *     another call, lastStepTimeTriggered is true, divPosition is zero, we schedule the next step
 *     
 *     example divider is 3/4x:
 *     same as above but...
 *     
 *     
 */

 
// possibly schedule steps on all tracks to play
static inline void
scheduleNext(void)
{
   // This means that we have an actual new step, and we need to
   // obey it. If we don't have this, we interpolate when clock
   // divider is 4x or 2x (or 3/4). when we have an actual step
   // also, if clock divider is 1/4 or 1/2 (or 3/4), we increment
   // a counter but don't schedule a step
   uint8_t haveRealStep = gRunningState.lastStepTimeTriggered;      
   gRunningState.lastStepTimeTriggered = 0;
   
   // for each track check it
   for (uint8_t i = 0; i<NUM_TRACKS; i++) {
      TrackRunningState *trState = &gRunningState.trackStates[i];
      SeqTrack *track = &gSeqState.tracks[i];

      
      // only be concerned with ones that are not already sched.
      if (!trState->isScheduled) { 

         if (haveRealStep) { // got a real start of step   

            // do we need a new random? if so generate it for the track
            // todo make sure it's explicit that randomRegen is 1 based or fix this
            if (trState->position % gSeqState.randomRegen == 0) {
               trState->lastRandom = rndRandom(16);
               LOGMESSAGE(1,"Regenerate random for track %d ... %d", i, trState->lastRandom);
            }


            if (!trState->denomCycle) {
               uint16_t swing = 0;
               // squirrel this away so that we can calculate an offset from
               // this time for our 1/4, 1/2 and 3/4 divs
               trState->lastDivStartTime = gRunningState.lastStepTime;

               // apply swing only when no clock divider and on odd 16th notes
               // swing is a value of 1..100 where 100 would put it right on
               // top of the next step
               // note that you can't have negative swing!
               if (gSeqState.swing &&
                  (trState->position % 2 == 1) &&
                  track->clockDividerNum == 0 &&
                  track->clockDividerDenom == 0) {
                  // a step's length in ms. there are 4 steps per beat
                  // SL = 1000 / ((gSeqState.tempo / 60) * 4)
                  // then swing is a percentage, so HSL * swing / 100
                  // this produced an (possibly invalid) overflow warning
                  //swing = (uint16_t)(1000 * 60 / 4 / 100) *gSeqState.swing / gSeqState.tempo;                                       
                  swing = (uint16_t)(150) * (uint16_t)gSeqState.swing / (uint16_t)gSeqState.tempo;                                       
               }

               //schedule it.
               // note that if tempo increases, we will just end up scheduling
               // these in the past which means they will play right away and we
               // will catch up (I think I observed this, but who knows if I'm missing something...)
               trState->nextScheduledStart = gRunningState.lastStepTime+swing;
               LOGMESSAGE(1, "Scheduled track %d for %d", i + 1, trState->nextScheduledStart);
               trState->isScheduled = 1;
               trState->numCycle = 0; // allow interpolation again
            } // if !denomCycle
         } // if haveRealStep
         else { // interpolate
            // if we've already interpolated all the intermediate
            // steps before the next whole step, we will not do so
            // again. Instead wait for a real step. This handles the
            // case of tempo slowing down (we'd want to wait for the next
            // real clock based step before advancing our track step ptr)
            if (trState->numCycle < track->clockDividerNum) {
               trState->numCycle++;

               // length of time between steps will be
               // 1000 / ((tempo/60)*4*(numerator/denominator))
               // that is multiplied with the cycle number we are on
               // to get the length of time from the last real step
               uint32_t periodLength =
                  (uint32_t)(15000 /*1000 * 60 / 4*/) * (track->clockDividerDenom + 1) 
                     * trState->numCycle
                     / (gSeqState.tempo*(track->clockDividerNum + 1));

               trState->nextScheduledStart = trState->lastDivStartTime +
                  (uint16_t)periodLength;
               trState->isScheduled = 1;               
            }
         }
      }

      // any time we have a real step, our denom cycle increments.
      // this allows us to skip steps when we are 1/4 1/2 or 3/4
      if (haveRealStep) {
         trState->denomCycle++;
         if (trState->denomCycle > track->clockDividerDenom)
            trState->denomCycle = 0;
      }
   }
}


// Given a step, will determine whether it should play based on random
// setting, and if so, play it
static inline void
processStep(uint8_t trNum, uint8_t pos)
{
   SeqTrack *track = &gSeqState.tracks[trNum];   
   SeqStep *step= &gSeqState.patterns[gRunningState.pattern].trackSteps[trNum].steps[pos];
   if (step->velocity) { // 0 velocity means it's not on
      // lastRandom is a number 0..15
      // prob of 0 doesn't mean never (it's effectively 1 based) 
      if (step->probability + 1 > gRunningState.trackStates[trNum].lastRandom) {
         uint8_t vel; 
         // 1..15 translate to 1..127. 
         vel = 7 + (step->velocity * 8);
         
         LOGMESSAGE(0, "[play] pat %1d tr %1d st %2d: ch %d note %d vel %d",
            gRunningState.pattern + 1, trNum + 1, pos + 1,
            track->midiChannel + 1, track->midiNote, vel);
         
         // This expects 1 based midi channel
         midiPlayNote(track->midiChannel + 1, track->midiNote, vel);         
      }
   }
}

// check to see if any scheduled steps should be played, and if so play them
static inline void
playNext(void)
{
   uint8_t chg=0;
   for (uint8_t i = 0; i<NUM_TRACKS; i++) {
      TrackRunningState *trState = &gRunningState.trackStates[i];
      SeqTrack *track = &gSeqState.tracks[i];
      
      // only concerned with ones that are not already sched.
      // on unmuted tracks
      if (trState->isScheduled &&
         (int16_t)(millis()-trState->nextScheduledStart) >= 0 ) {

         // play the step if need be
         if(!track->muted)
            processStep(i, trState->position);
         advancePosition(i);
         trState->isScheduled = 0;
      }    
   }
}

