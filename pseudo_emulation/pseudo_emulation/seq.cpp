#include "main.h"

// represents "real" time
// we will get a tick every n of these
int currentTime = 0;
#define TIME_PER_TICK 10 // 10 "milliseconds" per tick




uint32_t millis()
{
   return currentTime;
}
void midiSendClock()
{

}

/////////////NATIVE HERE///////////////////////
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
void scheduleNext(void)
{
   // swing is only applied when clock div <= 1
   for (uint8_t i = 0; i<NUM_TRACKS; i++) {
      SeqTrack *track = &gSeqState.patterns[gRunningState.currentPattern].tracks[i];
      TrackRunningState *trState = &gRunningState.trackPositions[i];

      gSeqState.swing;
      gSeqState.tempo; // if clock div, use to calculate position in time
      gRunningState.lastStepTime; // when we received the last step start tick
      track->clockDivider; //0=default (1x) sets speed 0=1,1=2,2=4x or 3=1/2, 4=1/4, 5=1/8x
      track->numSteps;
      track->muted;
      trState->position; // which step will be next (5 bits)
      trState->divPosition; // may be 0-3 if divx4 or 0-1 if divx2
      trState->isScheduled;  // whether or not we have scheduled the next note (reset to 0 when it's been played)
      trState->nextScheduledStart;// when it's scheduled to play (need to set this)



   }


}

void loop()
{

}

/////////////NATIVE END ///////////////////////
int xxCurrentTrack=0; 


void advanceClock()
{
   currentTime++;
   printf("clock time advances to %d\n", currentTime);
   // will get a tick for every advance
   internalClock();

}

char *cmds[] = {
   "q",        // 0 quit
   "clock",    // 1 advance clock n times default once
   "div",      // 2 set divider <num> <denom>
   "len",      // 3 set track len <len>
   "tempo",    // 4 set tempo
   "show",     // 5 show everything
   "loop",     // 6 another loop passes... (without time advancing)
   "track",    // 7 set track to <n> (0 based)
   
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

void doSequencer()
{
   int c;
   int params[100] = {};
   int pc;

   SeqPattern *statePattern;
   SeqTrack *stateTrack;
   TrackRunningState *runTrack;
  
   for (;;) {

      statePattern = &gSeqState.patterns[0];
      stateTrack = &statePattern->tracks[xxCurrentTrack];
      runTrack = &gRunningState.trackPositions[xxCurrentTrack];

      c = getCommand(params, &pc);
      switch (c) {
      case -1:
         printf("unknown\n");
         break;
      case 0: // quit
         goto end;
      case 1: // clock advance n times
         if (!pc)
            params[0] = 1;
         while (params[0]--) {
            advanceClock();
            scheduleNext();
            // todo check play
         }
         break;
      case 2: // set divider num/denom
      {
         int num = params[0];
         int denom = params[1];
         //0=default (1x) sets speed 0=1, 1=2, 2=4x or 3=1/2, 4=1/4, 5=1/8x, 6=3/4
         if (num == 1) {
            if (denom == 1) //1/1
               stateTrack->clockDivider = 0;
            else if (denom == 2) //1/2
               stateTrack->clockDivider = 3;
            else if (denom == 4) //1/4
               stateTrack->clockDivider = 4;
            else if (denom == 8) // 1/8
               stateTrack->clockDivider = 5;
         }  else if (num == 3 && denom == 4) { //3/4
            stateTrack->clockDivider = 6;
         }
         else if (num == 2 && denom == 1) { //2/1
            stateTrack->clockDivider = 1;
         }
         else if (num == 4 && denom == 1) { //4/1
            stateTrack->clockDivider = 2;
         }
         else {
            printf("invalid clock div\n");
         }
      }
      break;
      case 3: // len
         if (params[0] > 32 || params[0] < 1) {
            printf("Invalid len (1-32)\n");
         } else {
            stateTrack->numSteps = params[0] - 1;
         }

         break;
      case 4: // tempo
         if (params[0] > 255)
            printf("Invalid tempo (0-255)\n");
         else
            gSeqState.tempo = params[0];
         break;
      case 6: // loop
         loop();
         break;
      }
   }

end:
   ;
}
