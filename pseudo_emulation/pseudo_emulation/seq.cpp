#include "main.h"
#include <stdarg.h>
#define DEFAULT_LOG_FILE "c:\\crap\\seqlog.txt"
#define COPY_LOG_FILE_SPEC "c:\\crap\\seqlog%d.txt"
// state for this program (non-native)

// represents "real" time in microseconds
int xxCurrentTime = 0;
FILE *xxLog;
FILE *xxInputStream;

void fcopy(FILE *f1, FILE *f2)
{
   char            buffer[BUFSIZ];
   size_t          n;

   while ((n = fread(buffer, sizeof(char), sizeof(buffer), f1)) > 0)
   {
      if (fwrite(buffer, sizeof(char), n, f2) != n)
         printf("write failed\n");
   }
}


void fileCopy(const char *s, const char *d)
{
   FILE *fp1;
   FILE *fp2;

   if ((fp1 = fopen(s, "rb")) == 0)
      printf("cannot open file %s for reading\n", s);
   if ((fp2 = fopen(d, "wb")) == 0)
      printf("cannot open file %s for writing\n", d);
   fcopy(fp1, fp2); 
   fclose(fp1);
   fclose(fp2);
}


void logMessage(const char *format, ...)
{
   static int last = 0;
   va_list args;
   printf("%5d (el:%4d): ", xxCurrentTime/1000, (xxCurrentTime / 1000)-last);
   last = xxCurrentTime / 1000;
   va_start(args, format);
   vprintf(format, args);
   va_end(args);
   printf("\n");
}
uint32_t millis()
{
   return xxCurrentTime / 1000;
}
void midiSendClock()
{

}
void playStep(int track, int position, int chan, int note, int vel)
{
   // todo random check, mute check, send midi note
   logMessage("[play] pat %1d tr %1d st %2d: ch %d note %d vel %d", 
      gRunningState.pattern + 1, track + 1, position + 1,
      chan+1, note, vel);
}

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
      logMessage("Random thoughts...");
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

//////////////////////////////////////////////////////////////////////////////////////
//// From timer/////
// this will (in addition to handling internal clock) be responsible
// for sending out a midi clock tick if enabled
static uint8_t stepCounter;

/* This gets called when timer1 ticks. This corresponds to BPM so it
*  will get called 24 times per quarter note.
*  It will not be called at all when we are in midi slave mode
*  It's purpose is to just set lastStepTime
*/
void internalClock()
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

   logMessage("[pattern switch] pattern %d", pattern + 1);
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
   gSeqState.tempo = DEFAULT_BPM;

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


// ensure that next clock tick starts at step boundary
// (user just hit start)
void seqSetTransport(uint8_t state)
{
   if (state == TRANSPORT_STARTED) {
      // this means that when you hit start, you could have a miniscule delay until next timer click
      stepCounter = 0;
      // initialize current pattern
      switchToPattern(NUM_PATTERNS);
   }
   gRunningState.transportState = state;

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

   printf("Sprunt!\n"); // should never see this
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
               logMessage("Regenerate random for track %d ... %d", i, track->lastRandom);
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

//
// MAIN LOOP (native)
//
void loop()
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

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
/////////////NATIVE END //////////////////////////////////////////////////////////////////////


void advanceClock(int increment)
{
   // internalClock is called every n milliseconds
   // where n is the period of time between a 24 ppqn beat
   // (same as midi clock)
   // we need to use microseconds to fix rounding issues
   int period = ( 1000 * 1000 / (gSeqState.tempo * 24 / 60) );
   increment *= 1000; // convert to uS
   while (increment--) {
      // emulate the timer
      if (xxCurrentTime % period == 0) {
         internalClock();
      }
      
      // call main loop
      if((increment % 1000)==0)
         loop();
      xxCurrentTime++;
   }
   printf("\n");


}

struct Command {
   const char *cmd;
   const char *help;
};

Command cmds[] = {
   ////////////////////////////////////////////////////////////////////////////////]
   {"q","quit"}, //0
   {"cl","<n> advance clock n milliseconds default one" }, //1
   {"div","<num> <denom> set divider num/denom"}, //2
   {"len","<len> set track len (pass 1 based)" }, //3
   {"tempo","<n> set tempo to n" }, //4
   {"show", "<n> show everything. if n is 1 it shows track shit too" }, //5
   {"track", "<n> set track to <n> (pass 1 based)" }, //6
   {"play",  "<n> where n is 0=stop, 1=play" }, //7
   {"random","<n> generate a number from 0 to n-1" }, //8
   {"mute", "<n> <state> Mute/unmute track n (1 based) state is 0=unmute 1=mute" }, //9
   {"pat", "<n> <d> pattern switch to pattern n=1..4, d=delay switch (1=yes/0=no)"}, //10
   {"save", "<n> save all commands thusfar to file n"}, //11
   {"load", "<n> run commands from file n" }, //12
   {"midi", "<n> set midi note 0..127 for current track"}, //13
   {"step", "<stepnum> <vel> <prob> set vel (0..15), prob (1..16) vel 0=off"}, //14
   {"qstep","<n n n...> Turn steps on/off (0=off)"}, //15
   {"regen","<n> Regenerate random every n steps (1..32)" }, //16
   { "swing","<n> set swing to n (0..100)" }, //17
   
   { 0,0 }
};

void showCommands()
{
   Command *c = cmds;
   while (c->cmd) {
      printf("%6s - %s\n", c->cmd, c->help);
      c++;
   }

}

int lookupCommand(const char *cmd)
{
   int ctr = 0;
   Command *c = cmds;
   while (c->cmd) {
      if (!strcmp(cmd, c->cmd))
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
   char savebuf[256];
   char *pos, *word;
   *pc = 0; // no parameters
   int token = 0;
reRead:
   if (0 == fgets(cmdbuf, 255, xxInputStream)) {
      if (feof(xxInputStream)) {
         if (xxInputStream != stdin) {
            fclose(xxInputStream);
            xxInputStream = stdin;
            goto reRead;
         }
      }
   }
   if (xxInputStream != stdin) {
      printf("%s", cmdbuf);
   }

   strcpy(savebuf, cmdbuf);
   
   memset(params, 0, 100 * sizeof(int));
   pos = cmdbuf;
   word = cmdbuf;
   bool theEnd = false;
   while(!theEnd) {   
      if (!(*pos))
         theEnd = true;
      if (!(*pos) || *pos == ' ' || *pos == '\r' || *pos == '\n') {
         *pos = 0;
         if (!token) { // first         
            ret = lookupCommand(word);
         }
         else {
            params[*pc] = atoi(word);
            (*pc)++;
         }
         token++;
         if (token > 99)
            break;
         pos++;
         word = pos;
      }
      else
         pos++;      
   }

   // only save it if it's valid and is not load or save or quit
   if (ret != -1 && ret != 11 && ret != 12 && ret != 0) {
      fputs(savebuf, xxLog);
   }

   return ret;
         
}

void showState(bool track)
{
   printf("[Pat %d Trk %d Time %dms]\n"
      "Tempo %d, swing %d (clock every %dms, 16th step every %dms) %s\n",
      gRunningState.pattern + 1,
      gRunningState.currentTrack + 1,
      xxCurrentTime / 1000,

      (int)gSeqState.tempo,
      gSeqState.swing,
      (1000*60/24) / gSeqState.tempo,
      (1000 *60/4) / gSeqState.tempo ,
      gRunningState.transportState ==TRANSPORT_STARTED ?  "started" : "stopped"
   );
   printf("Regen every %d steps. MIDI channel %d\n", gSeqState.randomRegen, gSeqState.midiChannel);
   if (gRunningState.nextPattern < NUM_PATTERNS) {
      printf("Next pattern %d (%s)\n", gRunningState.nextPattern + 1,
         gSeqState.delayPatternSwitch ? "delay" : "immediate");
   }

   for (int i = 0; i < NUM_TRACKS; i++) {
      printf("Track %d: steps: %d divider(%d/%d) MIDI %d %s\n", i + 1,
         gSeqState.patterns[gRunningState.pattern].tracks[i].numSteps + 1,
         gSeqState.patterns[gRunningState.pattern].tracks[i].clockDividerNum+1,
         gSeqState.patterns[gRunningState.pattern].tracks[i].clockDividerDenom + 1,
         gSeqState.patterns[gRunningState.pattern].tracks[i].midiNote,
         gSeqState.patterns[gRunningState.pattern].tracks[i].muted ? "muted" : ""
         );
   }

   if (track) {
      int i;
      printf("Track state (top row velocity bottom row prob)\n");
      SeqTrack *tr = &gSeqState.patterns[gRunningState.pattern].tracks[gRunningState.currentTrack];
      for (int cycle = 0; cycle < 2; cycle++) {
         int start = cycle * 16;
         int len = start + 16;
         if (len > tr->numSteps+1)
            len = tr->numSteps+1;
         for (i = start; i < len; i++) {
            printf("%02d_", i + 1);
         }
         if(start<len)
            printf("\n");
         for (i = start; i < len; i++) {
            if (tr->steps[i].velocity)
               printf("%02d ", tr->steps[i].velocity);
            else
               printf("   ");
         }
         if (start<len)
            printf("\n");
         for (i = start; i < len; i++) {
            if (tr->steps[i].velocity)
               printf("%02d ", tr->steps[i].probability+1);
            else
               printf("   ");
         }
         if (start<len)
            printf("\n");
      }
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
   xxInputStream = stdin; // reading cmds from stdin
   xxLog = fopen(DEFAULT_LOG_FILE, "w");
   if (!xxLog) {
      printf("Failed to create " DEFAULT_LOG_FILE "\n");
      return;
   }
   seqSetup();

   for (;;) {

      statePattern = &gSeqState.patterns[gRunningState.pattern];
      stateTrack = &statePattern->tracks[gRunningState.currentTrack];
      runTrack = &gRunningState.trackPositions[gRunningState.currentTrack];

      printf("%dms >", xxCurrentTime / 1000);
      c = getCommand(params, &pc);
      switch (c) {
      case -1:
         printf("Available commands:\n");
         showCommands();
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
            stateTrack->clockDividerNum = num - 1;
            stateTrack->clockDividerDenom = denom - 1;
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
         showState(params[0] != 0);
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
         if (params[0] == TRANSPORT_STOPPED || params[0] == TRANSPORT_STARTED)
            seqSetTransport(params[0]);
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
      case 10: // switch pat
         if (params[0] < 1 || params[0] > NUM_PATTERNS ||
            params[1] < 0 || params[1] > 1)
            printf("Invalid pattern or delay setting\n");
         else {
            seqSetPattern(params[0] - 1);
            gSeqState.delayPatternSwitch = params[1];
         }
         break;
      case 11: // save off all commands thusfar 
      {
         char fnbuf[512];
         fclose(xxLog);
         sprintf(fnbuf, COPY_LOG_FILE_SPEC, params[0]);
         fileCopy(DEFAULT_LOG_FILE, fnbuf);
         xxLog = fopen(DEFAULT_LOG_FILE, "a");
         break;
      }
      case 12: // play back log
      {
         char fnbuf[512];
         sprintf(fnbuf, COPY_LOG_FILE_SPEC, params[0]);
         xxInputStream = fopen(fnbuf, "r");
         if (!xxInputStream) {
            xxInputStream = stdin;
            printf("File open failed for %s\n", fnbuf);
         }
         break;
      }
      case 13: // set midi note for track
         if (params[0] >= 0 && params[0] < 128)
            stateTrack->midiNote = params[0];
         else
            printf("You dullard! Enter a proper range\n");
         break;
      case 14: // step vel prob 0..15 1..16
         if (params[0] >= 1 && params[0] <= NUM_STEPS && 
            params[1] >= 0 && params[1] < 16 && 
            params[2] >= 1 && params[2] <= 16) {

            stateTrack->steps[params[0]-1].velocity = params[1];
            stateTrack->steps[params[0] - 1].probability = params[2]-1;
         }
         else {
            printf("Out of range for one of those. Try again\n");
         }
         break;
      case 15: // qstep
         for (int qstep_i = 0; qstep_i < pc && qstep_i < NUM_STEPS; qstep_i++) {
            if (params[qstep_i]) {
               stateTrack->steps[qstep_i].velocity = 15;
               stateTrack->steps[qstep_i].probability = 15;
            }
            else {
               stateTrack->steps[qstep_i].velocity = 0;
               stateTrack->steps[qstep_i].probability = 0;
            }
         }
         break;
      case 16: // regen cycles
         if (params[0] >= 1 && params[0] <= NUM_STEPS)
            gSeqState.randomRegen = params[0];
         else
            printf("Range is 1..32\n");
         break;
      case 17: // swing
         if (params[0] >= 0 && params[0] <= 100)
            gSeqState.swing = params[0];
         else
            printf("Range is 0..100\n");
         break;
      
      } // end case
   } // end for(;;)

end:
   ;
}
