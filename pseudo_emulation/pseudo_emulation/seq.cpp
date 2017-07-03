#include "main.h"
#include <stdarg.h>

// state for this program (non-native)

// represents "real" time in milliseconds
int xxCurrentTime = 0;


void logMessage(const char *format, ...)
{
   va_list args;
   printf("%5d: ", xxCurrentTime);
   va_start(args, format);
   vprintf(format, args);
   va_end(args);
   printf("\n");
}



// RANDOMNESS /////////////////////////////////////////////////////////
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
      logMessage("Random thoughts...");
   }
}

///////////////END RANDOM RAB//////////////////////////////////////////////////////////

uint8_t random(uint8_t max)
{
   static bool first = true;
   static pcg8i_random_t rng;
   if (first) {
      first = false;
      // todo these need to be randomly seeded
      pcg8i_srandom_r(&rng, 42u, 54u);
   }
   return pcg8i_boundedrand_r(&rng, max);
}


uint32_t millis()
{
   return xxCurrentTime;
}
void midiSendClock()
{

}


void playStep(int track, int position)
{
   // todo random check, mute check, send midi note
   logMessage("[play] track %d step %d", track + 1, position + 1);
}

/////////////NATIVE (AVR) HERE///////////////////////
//// From timer/////
// this will (in addition to handling internal clock) be responsible
// for sending out a midi clock tick if enabled
static uint8_t stepCounter;

/* This gets called when timer1 ticks. This corresponds to BPM so it
*  will get called 96 times per quarter note.
*  It will not be called at all when we are in midi slave mode
*  It's purpose is to just set lastStepTime
*/
void internalClock()
{
   /* Midi clock is sent 24 times per quarter note.
   * Since we get called 96 times per qn, only send every 4th time
   */
   if ((stepCounter % 4 == 0) &&
      gSeqState.midiSendClock &&
      gRunningState.transportState == TRANSPORT_STARTED) {
      midiSendClock();
   }

   // step occurs every 24 ticks (96/4 as there are 4 steps per beat)
   if ((stepCounter % 24) == 0) {
      gRunningState.lastStepTime = millis();
      gRunningState.lastStepTimeTriggered = 1;
   }

   stepCounter++;
   if (stepCounter > 23)
      stepCounter = 0;

}

// ensure that next clock tick starts at step boundary
// (user just hit start)
void clockSetStarted(void)
{
   // this means that when you hit start, you could have a miniscule delay until next timer click
   stepCounter = 0;

}
//// From seq ////

void resetTracks()
{
   // prepare tracks to be scheduled
   memset(gRunningState.trackPositions, 0, NUM_TRACKS * sizeof(TrackRunningState));
}


// switch to a new pattern. this also 
// resets each track on the new pattern to be ready for scheduling
// TODO this could be called when starting transport?
void switchToPattern(uint8_t pattern)
{
   gRunningState.pattern = pattern;
   gRunningState.patternCurrentCycle = 0;
   gRunningState.patternHasPlayed = 0;
   resetTracks();
}

// randomly determine which pattern will play next
// returns NUM_PATTERNS if no switch will happen
uint8_t determineNextPattern()
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

   printf("Sprunt!\n"); // should never see this
   exit(0);

   return NUM_PATTERNS;
}

// check to see if we need to change to a new pattern.
// if so, figure out what pattern we need
void patternChangeCheck()
{
   uint8_t hasPlayed = gRunningState.patternHasPlayed;
   
   // reset this bit always
   gRunningState.patternHasPlayed = 0;

   if (gRunningState.patternHold) {
      // don't switch patterns at all, and don't increment the current cycle
      // (playStep is responsible for cycling back to beginning of current pattern)
      return;
   }

   if (hasPlayed) {
      // increment the counter of how many times we've played it
      gRunningState.patternCurrentCycle++;
      // see if we need to switch
      if (gRunningState.patternCurrentCycle >
         gSeqState.patterns[gRunningState.pattern].numCycles) {
         uint8_t newPattern;
         // we need to switch. figure out which pattern to switch to
         newPattern = determineNextPattern();
         if(newPattern < NUM_PATTERNS)
            switchToPattern(newPattern);
      }
         
   }
   else {  // not at end of cycle. 
      // the only time we switch is at user request
      if (gRunningState.nextPattern < NUM_PATTERNS &&
         !gSeqState.delayPatternSwitch) {
         switchToPattern(gRunningState.nextPattern);
         gRunningState.nextPattern = NUM_PATTERNS; // reset this
      }
   }
}

// move track position to next
// flagging "pattern has played" flag if need be
inline void advancePosition(uint8_t tr)
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

// possibly schedule steps on all tracks to play
void scheduleNext(void)
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
         /* simplest case for 1/1 time
         if (haveRealStep) {
            trState->nextScheduledStart = gRunningState.lastStepTime;
            trState->isScheduled = 1;
         }*/
         if (haveRealStep) { // got a real start of step            
            if (!trState->denomCycle) {
               uint8_t swing = 0;

               // apply swing only when no clock divider and on odd 16th notes
               // swing is a value of 1..100 where 100 would put it right on
               // top of the next step
               // note that you can't have negative swing!
               if (gSeqState.swing &&
                  (trState->position % 2 == 1) &&
                  track->clockDividerNum == 1 &&
                  track->clockDividerDenom == 1) {
                  uint16_t halfStepLength =
                     1000 / ((gSeqState.tempo / 60) * 8);
                  swing = halfStepLength * gSeqState.swing / 100;
               }

               //schedule it
               trState->nextScheduledStart = gRunningState.lastStepTime+swing;
               trState->isScheduled = 1;

               // if tempo increased and we are 2x or 4x, we might not have completed playing
               // the intermediate positions for a whole step. in this case we want to 
               // throw away the extra ones
               if (trState->numCycle &&
                  trState->numCycle < track->clockDividerNum) {
                  // need to throw away positions without playing them
                  while (trState->numCycle < track->clockDividerNum) {
                     advancePosition(i);
                     trState->numCycle++;
                  }
               }
               trState->numCycle = 0; // allow interpolation again
            }

            trState->denomCycle++;
            if (trState->denomCycle > track->clockDividerDenom)
               trState->denomCycle = 0;
         }
         else { // interpolate
            // if we've already interpolated all the intermediate
            // steps before the next whole step, we will not do so
            // again. Instead wait for a real step. This handles the
            // case of tempo slowing down (we'd want to wait for the next
            // real clock based step before advancing our track step ptr)
            if (trState->numCycle < track->clockDividerNum) {
               // length of time between 
               // todo make sure we have best precision and dont overflow
               //1000 / ((tempo/60)*4*(numerator/denominator))
               uint16_t periodLength =
                  1000 / ((gSeqState.tempo / 60) * 4 *
                  ((track->clockDividerNum + 1) / (track->clockDividerDenom + 1)));

               trState->numCycle++;
               trState->nextScheduledStart = gRunningState.lastStepTime +
                  (periodLength*trState->numCycle);
               trState->isScheduled = 1;
            }
         }
      }

      // swing is only applied when clock div <= 1
      gSeqState.swing;
      
      trState->position; // which step will be next (5 bits)
      trState->numCycle;
      trState->denomCycle; // may be 0-3 if divx4 or 0-1 if divx2
      trState->isScheduled;  // whether or not we have scheduled the next note (reset to 0 when it's been played)
      trState->nextScheduledStart;// when it's scheduled to play (need to set this)



   }


}



void playNext(void)
{
   uint8_t chg=0;
   for (uint8_t i = 0; i<NUM_TRACKS; i++) {
      TrackRunningState *trState = &gRunningState.trackPositions[i];
      SeqTrack *track = &gSeqState.patterns[gRunningState.pattern].tracks[i];

      // only concerned with ones that are not already sched.
      // on unmuted tracks
      if (trState->isScheduled &&
         (int16_t)(millis()-trState->nextScheduledStart) >= 0 ) {

         // >>>code to play step goes here
         playStep(i, trState->position);

         advancePosition(i);
         

         trState->isScheduled = 0;


      }

      gSeqState.swing;
      gSeqState.tempo; // if clock div, use to calculate position in time
      gRunningState.lastStepTime; // when we received the last step start tick
      track->clockDividerNum;
      track->clockDividerDenom;
      track->numSteps;

      trState->position; // which step will be next (5 bits)
      trState->numCycle; 
      trState->denomCycle;
      trState->isScheduled;  // whether or not we have scheduled the next note (reset to 0 when it's been played)
      trState->nextScheduledStart;// when it's scheduled to play (need to set this)



   }
}

void loop()
{
   if (gRunningState.transportState == TRANSPORT_STARTED) {
      // do we need to schedule any steps?
      scheduleNext();
      // do we need to play any scheduled steps?
      playNext();
      // do we need to change patterns?
      patternChangeCheck();
   }
}

///////////////////////////////////////////////
///////////////////////////////////////////////
/////////////NATIVE END ///////////////////////


void advanceClock(int increment)
{
   // todo with rounding this is inaccurate
   // internalClock is called every n milliseconds
   int period = (1000 / ((gSeqState.tempo / 60) * 96));
   while (increment--) {
      // emulate the timer
      if ((xxCurrentTime % period) == 0) {
         internalClock();
      }
      // call main loop
      loop();
      xxCurrentTime++;
   }
   printf("\n");


}

char *cmds[] = {
   "q",        // 0 quit
   "cl",       // 1 <n> advance clock n milliseconds default one
   "div",      // 2 <num> <denom> set divider num/denom
   "len",      // 3 <len> set track len (pass 1 based)
   "tempo",    // 4 <n> set tempo to n
   "show",     // 5 show everything
   "track",    // 6 <n> set track to <n> (pass 1 based)
   "play",     // 7 <n> where n is 0=stop, 1=play
   "random",   // 8 <n> generate a number from 0 to n-1   
   "mute",     // 9 <n> <state> Mute/unmute track n (1 based) state is 0=unmute 1=mute
   0
};

int lookupCommand(const char *cmd)
{
   int ctr = 0;
   char **c = cmds;
   while (*c) {
      if (!strcmp(cmd, *c))
         return ctr;
      ctr++;
      c++;
   }
   return -1;
}

// return command
int getCommand(int *params, int *pc)
{
   int ret = 0;
   char cmdbuf[256];
   char *pos, *word;
   *pc = 0; // no parameters
   int token = 0;
   fgets(cmdbuf, 255, stdin);

   pos = cmdbuf;
   word = cmdbuf;
   while (*pos) {
      if (*pos == ' ' || *pos == '\r' || *pos == '\n') {
         *pos = 0;
         if (!token) { // first         
            ret = lookupCommand(word);
         }
         else {
            params[*pc] = atoi(word);
            (*pc)++;
         }
         token++;
         if (token > 2)
            break;
         pos++;
         word = pos;
      }

      else
         pos++;
   }


   return ret;
         
}

void showState()
{
   printf("[Pat %d Trk %d Time %dms]\n"
      "Tempo %d (clock every %dms, 16th step every %dms) %s\n",
      gRunningState.pattern+1, 
      gRunningState.currentTrack+1,
      xxCurrentTime,

      (int)gSeqState.tempo,
      (1000 / ((gSeqState.tempo / 60) * 96)),
      (1000 / ((gSeqState.tempo / 60) * 4)),
      gRunningState.transportState ==TRANSPORT_STARTED ?  "started" : "stopped"
   );

   for (int i = 0; i < NUM_TRACKS; i++) {
      printf("Track %d: steps: %d divider(%d/%d) MIDI %d %s\n", i + 1,
         gSeqState.patterns[gRunningState.pattern].tracks[i].numSteps + 1,
         gSeqState.patterns[gRunningState.pattern].tracks[i].clockDividerNum+1,
         gSeqState.patterns[gRunningState.pattern].tracks[i].clockDividerDenom + 1,
         gSeqState.patterns[gRunningState.pattern].tracks[i].midiNote,
         gSeqState.patterns[gRunningState.pattern].tracks[i].muted ? "muted" : ""
         );
   }

}

void doSequencer()
{
   int c;
   int params[100] = {};
   int pc;

   SeqPattern *statePattern;
   SeqTrack *stateTrack;
   TrackRunningState *runTrack;
   gSeqState.tempo = 120;
  
   for (;;) {

      statePattern = &gSeqState.patterns[gRunningState.pattern];
      stateTrack = &statePattern->tracks[gRunningState.currentTrack];
      runTrack = &gRunningState.trackPositions[gRunningState.currentTrack];

      c = getCommand(params, &pc);
      switch (c) {
      case -1:
         printf("unknown\n");
         break;
      case 0: // quit
         goto end;
      case 1: // clock advance n milliseconds
         if (!pc)
            params[0] = 1;
         advanceClock(params[0]);
         break;
      case 2: // set divider num/denom
      {
         int num = params[0];
         int denom = params[1];
         // todo others are unsupported
         if (num < 1 || num >4 || denom < 1 || denom >4 || denom == 3) {
            printf("unsupported clock divider\n");
         }
         else {
            stateTrack->clockDividerNum = num;
            stateTrack->clockDividerDenom = denom;
         }

      }
      break;
      case 3: // len
         if (params[0] > 32 || params[0] < 1) {
            printf("Invalid len (1-32)\n");
         }
         else {
            stateTrack->numSteps = params[0] - 1;
         }

         break;
      case 4: // tempo
         if (params[0] > 255 || params[0] < 1)
            printf("Invalid tempo (1-255)\n");
         else
            gSeqState.tempo = params[0];
         break;
      case 5: // show everything
         showState();
         break;
      case 6: // set current track (1 based)
         if (params[0] > 0 && params[0] < 7) {
            gRunningState.currentTrack = params[0] - 1;
            printf("Track set to %d\n", params[0]);
         }
         else {
            printf("Invalid track\n");
         }
         break;
      case 7: // transport 0=stop, 1=play
         if (params[0] == 0)
            gRunningState.transportState = TRANSPORT_STOPPED;
         else if (params[0] == 1) {
            gRunningState.transportState = TRANSPORT_STARTED;
            clockSetStarted();
         }
         else
            printf("Invalid state [0 stop, 1 start]\n");
         break;
      case 8: // random
         printf("random number from 0 to %d inclusive is: %d\n", params[0] - 1, random(params[0]));
         break;
      case 9: // mute a track
         if (params[0] > 0 && params[0] < 7 && params[1] >= 0 && params[1] <= 1) {
            gSeqState.patterns[gRunningState.pattern].tracks[params[0] - 1].muted =
               params[1];
            printf("Track %d %s\n", params[0], params[1] ? "muted" : "unmuted");
         }
         else {
            printf("Invalid track\n");
         }
         break;

      }
   }

end:
   ;
}
