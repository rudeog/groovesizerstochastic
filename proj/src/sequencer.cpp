#include "sequencer.h"

// This is x-platform code

//
// Globals
//
SequencerState gSeqState;
RunningState gRunningState;

// set initial sequencer and running state
void seqSetup(void)
{
  SeqStep *step;
  SeqTrack *track;
  // initial 0 state should be fine for most things
  gSeqState.randomRegen = 1;
  gSeqState.tempo=DEFAULT_BPM;

  for(uint8_t i=0;i<NUM_PATTERNS;i++) {
    for(uint8_t j=0;j<NUM_TRACKS;j++) {
      track=&gSeqState.patterns[i].tracks[j];
      track->numSteps=NUM_STEPS-1; // 0 based
      track->midiNote=DEFAULT_MIDI_NOTE;
    }    
  }
}

// example
#if 0
void playStep()
{
  if (seqCurrentStep == 0 || seqCurrentStep == 16 || seqCurrentStep == seqFirstStep)
  {
    if (recall)
    {
      goLoad = true; // set a flag that we can now load a patch
    }
    //deal with the follow actions if we're on the first step
    else if (seqCurrentStep == 0 && followAction != 0)
    {
      switch (followAction)
      {
      case 1: // play the next pattern      
        if (checkToc(nowPlaying + 1) && saved == true) // check if there is a pattern stored in the next location
        {
          cued = nowPlaying + 1; // cue the next patch
          goLoad = true;
        }
        break;
      case 2:
        if (saved == true) // we don't want to go over page breaks (30 is the second to last location on the page), and check if there is a pattern stored in the next location
        {
          cued = head; // cue the patch marked as the head
          goLoad = true;
        }
        break;
      }
    }
  }

  if (!checkMute(seqCurrentStep)) // don't play anything if the step is muted on the master page
  {
    lastStep = seqCurrentStep; // used to blink the LED for the current step
    for (byte i = 0; i < 12; i++) // for each of the tracks
    {
      if (fxFlam[i] == 1 && (seqCurrentStep % 2 == 0) && !checkStepFlam(i, seqCurrentStep))
      {
        if (!syncStarted)
          scheduleFlam(i, track[i].accentLevel);
        else
          scheduleFlamSlave(i, track[i].accentLevel);
      }
      if (checkStepAccent(i, seqCurrentStep))
      {
        midiA.sendNoteOff(track[i].midiNoteNumber, track[i].accentLevel, track[i].midiChannel);
        midiA.sendNoteOn(track[i].midiNoteNumber, track[i].accentLevel, track[i].midiChannel);
        if (checkStepFlam(i, seqCurrentStep)) // schedule the first flam if this step is marked as flam
        {
          if (!syncStarted)
            scheduleFlam(i, track[i].accentLevel);
          else
            scheduleFlamSlave(i, track[i].accentLevel);
        }
        else // if the this step is not marked as a flam, turn off any sceduled flams
        track[i].nextFlamPulse = 0;
      }
      else if (checkStepOn(i, seqCurrentStep))
      {
        midiA.sendNoteOff(track[i].midiNoteNumber, track[i].level, track[i].midiChannel);
        midiA.sendNoteOn(track[i].midiNoteNumber, track[i].level, track[i].midiChannel);
        if (checkStepFlam(i, seqCurrentStep)) // schedule the first flam if this step is marked as flam
        {
          if (!syncStarted)
            scheduleFlam(i, track[i].accentLevel);
          else
            scheduleFlamSlave(i, track[i].accentLevel);
        }
        else // if the this step is not marked as a flam, turn off any sceduled flams
        track[i].nextFlamPulse = 0;
      }
    }
  }

  do 
  {
    if (seqNextStep >= 0)
      seqCurrentStep = (((seqCurrentStep - seqFirstStep) + seqNextStep)%seqLength) + seqFirstStep; // advance the step counter
    else // seqNextStep is negative
    {
      if ((seqCurrentStep + seqNextStep) < seqFirstStep || (seqCurrentStep + seqNextStep) > 32)
        seqCurrentStep = seqLastStep + 1 + ((seqCurrentStep - seqFirstStep) + seqNextStep);
      else
        seqCurrentStep += seqNextStep;
    }
  }
  while (checkSkip(seqCurrentStep)); // do it again if the step is marked as a skip
}
#endif





























//
//
// NEW CODE !!!
//
//

/////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////NATIVE (AVR) HERE////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// RANDOMNESS //
// from pcg
struct pcg_state_setseq_8 {
   uint8_t state;
   uint8_t inc;
};

typedef struct pcg_state_setseq_8       pcg8i_random_t;
#define pcg8i_srandom_r                 pcg_setseq_8_srandom_r
#define PCG_DEFAULT_MULTIPLIER_8        141U
#define pcg8i_random_r                  pcg_setseq_8_rxs_m_xs_8_random_r
#define pcg8i_boundedrand_r             pcg_setseq_8_rxs_m_xs_8_boundedrand_r

inline uint8_t pcg_output_rxs_m_xs_8_8(uint8_t state)
{
   uint8_t word = ((state >> ((state >> 6u) + 2u)) ^ state) * 217u;
   return (word >> 6u) ^ word;
}
inline void pcg_setseq_8_step_r(struct pcg_state_setseq_8* rng)
{
   rng->state = rng->state * PCG_DEFAULT_MULTIPLIER_8 + rng->inc;
}

inline uint8_t pcg_setseq_8_rxs_m_xs_8_random_r(struct pcg_state_setseq_8* rng)
{
   uint8_t oldstate = rng->state;
   pcg_setseq_8_step_r(rng);
   return pcg_output_rxs_m_xs_8_8(oldstate);
}


inline void pcg_setseq_8_srandom_r(struct pcg_state_setseq_8* rng,
   uint8_t initstate, uint8_t initseq)
{
   rng->state = 0U;
   rng->inc = (initseq << 1u) | 1u;
   pcg_setseq_8_step_r(rng);
   rng->state += initstate;
   pcg_setseq_8_step_r(rng);
}

inline uint8_t
pcg_setseq_8_rxs_m_xs_8_boundedrand_r(struct pcg_state_setseq_8* rng,
   uint8_t bound)
{
   uint8_t threshold = ((uint8_t)(-bound)) % bound;
   for (;;) {
      uint8_t r = pcg_setseq_8_rxs_m_xs_8_random_r(rng);
      if (r >= threshold)
         return r % bound;
      logMessage(1,"Random thoughts...");
   }
}

// return 0..max-1
uint8_t random(uint8_t max)
{
   static bool first = true;
   static pcg8i_random_t rng;
   if (first) {
      first = false;
      // TODO these need to be randomly seeded
      pcg8i_srandom_r(&rng, 42u, 54u);
   }
   return pcg8i_boundedrand_r(&rng, max);
}
///////////////END RANDOM RAB//////////////////////////////////////////////////////////

 
//// From seq ////
// anything starting with seq is extern
static void resetTracks()
{
   // prepare tracks to be scheduled
   memset(gRunningState.trackPositions, 0, NUM_TRACKS * sizeof(TrackRunningState));
}

// switch to a new pattern. this also 
// resets each track on the new pattern to be ready for scheduling
static void switchToPattern(uint8_t pattern)
{
   if (pattern == NUM_PATTERNS) // don't switch, just initialize
      pattern = gRunningState.pattern;

   logMessage(0,"[pattern switch] pattern %d", pattern + 1);
   gRunningState.pattern = pattern;
   gRunningState.patternCurrentCycle = 0;
   gRunningState.patternHasPlayed = 0;
   gRunningState.nextPattern = NUM_PATTERNS;
   resetTracks();
}
void seqSetup(void)
{
   SeqTrack *track;
   // initial 0 state should be fine for most things
   gSeqState.randomRegen = 1;
   
   for (uint8_t i = 0; i<NUM_PATTERNS; i++) {
      for (uint8_t j = 0; j<NUM_TRACKS; j++) {
         track = &gSeqState.patterns[i].tracks[j];
         track->numSteps = DEFAULT_NUM_STEPS - 1; // 0 based
         track->midiNote = DEFAULT_MIDI_NOTE;
      }
   }

   switchToPattern(0);
}

// invoke whenever user switches pattern
void seqSetPattern(uint8_t pat)
{
   if (gRunningState.transportState == TRANSPORT_STARTED)
      gRunningState.nextPattern = pat;
   else // transport is stopped so do a full switch
      switchToPattern(pat);
}

// TODO should this be sitting here? used in 2 funcs below
static uint8_t stepCounter;
// ensure that next clock tick starts at step boundary
// (user just hit start)
void seqSetTransportState(uint8_t state)
{
   if (state == TRANSPORT_STARTED) {
      // this means that when you hit start, you could have a miniscule delay until next timer click
      stepCounter = 0;
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
   if (stepCounter  == 0) {
      gRunningState.lastStepTime = millis();
      gRunningState.lastStepTimeTriggered = 1;
   }

   stepCounter++;
   if (stepCounter > 5)
      stepCounter = 0;

}


// randomly determine which pattern will play next
// returns NUM_PATTERNS if no switch will happen
static uint8_t determineNextPattern()
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
   rnd = random(tot);

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

   outputString("Sprunt!\n"); // should never see this
   exit(0);

   return NUM_PATTERNS;
}

// check to see if we need to change to a new pattern.
// if so, figure out what pattern we need
void seqPatternChangeCheck()
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

// move track position to next
// flagging "pattern has played" flag if need be
static inline void advancePosition(uint8_t tr)
{
   TrackRunningState *trState = &gRunningState.trackPositions[tr];
   SeqTrack *track = &gSeqState.patterns[gRunningState.pattern].tracks[tr];

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
void seqScheduleNext(void)
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
      TrackRunningState *trState = &gRunningState.trackPositions[i];
      SeqTrack *track = &gSeqState.patterns[gRunningState.pattern].tracks[i];

      
      // only be concerned with ones that are not already sched.
      if (!trState->isScheduled) { 

         if (haveRealStep) { // got a real start of step   

            // do we need a new random? if so generate it for the track
            // todo make sure it's explicit that randomRegen is 1 based or fix this
            if (trState->position % gSeqState.randomRegen == 0) {
               track->lastRandom = random(16);
               logMessage(1,"Regenerate random for track %d ... %d", i, track->lastRandom);
            }


            if (!trState->denomCycle) {
               uint16_t swing = 0;
               // squirrel this away so that we can calculate an offset from
               // this time for our 1/4, 1/2 and 3/4 divs
               gRunningState.lastDivStartTime = gRunningState.lastStepTime;

               // apply swing only when no clock divider and on odd 16th notes
               // swing is a value of 1..100 where 100 would put it right on
               // top of the next step
               // note that you can't have negative swing!
               if (gSeqState.swing &&
                  (trState->position % 2 == 1) &&
                  track->clockDividerNum == 1 &&
                  track->clockDividerDenom == 1) {
                  // half a step's length in ms. there are 4 steps per beat
                  // HSL = 1000 / ((gSeqState.tempo / 60) * 8)
                  // then swing is a percentage, so HSL * swing / 100
                  swing = (uint16_t)(1000 * 60 / 8 / 100) *gSeqState.swing / 
                     gSeqState.tempo;                                       
               }

               //schedule it.
               // note that if tempo increases, we will just end up scheduling
               // these in the past which means they will play right away and we
               // will catch up (I think I observed this, but who knows if I'm missing something...)
               trState->nextScheduledStart = gRunningState.lastStepTime+swing;
               logMessage(1, "Scheduled track %d for %d", i + 1, trState->nextScheduledStart);
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
                  (uint32_t)(1000 * 60 / 4) * (track->clockDividerDenom + 1) 
                     * trState->numCycle
                     / (gSeqState.tempo*(track->clockDividerNum + 1));

               trState->nextScheduledStart = gRunningState.lastDivStartTime +
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


static void processStep(uint8_t trNum, uint8_t pos)
{
   SeqTrack *track = &gSeqState.patterns[gRunningState.pattern].tracks[trNum];
   SeqStep *step= &track->steps[pos];
   if (step->velocity) {
      //lastRandom is a number 0..15
      // prob of 0 doesn't mean never (effectively 1 based) 
      if (step->probability + 1 > track->lastRandom) {
         uint8_t vel; 
         // 1..15 translate to 1..127
         vel = 7 + (step->velocity * 8);
         playStep(trNum, pos, gSeqState.midiChannel, track->midiNote, vel);
      }

   }
}

void seqPlayNext(void)
{
   uint8_t chg=0;
   for (uint8_t i = 0; i<NUM_TRACKS; i++) {
      TrackRunningState *trState = &gRunningState.trackPositions[i];
      SeqTrack *track = &gSeqState.patterns[gRunningState.pattern].tracks[i];

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

// called from main loop to see if we need to alter sequencer state
void
seqCheckState(void)
{ 
   if (gRunningState.transportState == TRANSPORT_STARTED) {

      // do we need to schedule any steps?
      // we only do this when we've had a clock tick.
      // more than that would be a waste of cpu
      if (gRunningState.haveClockTick) {
         seqScheduleNext();
         gRunningState.haveClockTick = 0;
      }

      // do we need to play any scheduled steps?
      // do this more frequently because of swing and div times that
      // might require more granularity
      seqPlayNext();

      // do we need to change patterns?
      // needs to be called after the above
      seqPatternChangeCheck();
   }
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
