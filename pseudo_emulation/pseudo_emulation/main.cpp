#include "main.h"
// windows specific

// use this to figure out size at compile time (currently 835)
//char(*__kaboom)[sizeof(gSeqState)] = 1;
// currently 40
//char(*__kaboom2)[sizeof(gRunningState)] = 1;

#define DEFAULT_LOG_FILE "c:\\crap\\seqlog.txt"
#define COPY_LOG_FILE_SPEC "c:\\crap\\seqlog%d.txt"
#define SAVE_OUTPUT_FILE "c:\\crap\\seqoutput.txt"

// state for this program (non-native)

// represents "real" time in microseconds
int xxCurrentTime = 0;
// higher is more detailed (0..5)
int xxLogLevel = 1;
FILE *xxLog;
FILE *xxInputStream;
FILE *xxDumpFile;

void outputStringVA(const char *format, va_list valist)
{
   vprintf(format, valist);
   if (xxDumpFile)
      vfprintf(xxDumpFile, format, valist);
}

void outputString(const char *format, ...)
{
   static int last = 0;
   va_list args;
   va_start(args, format);
   outputStringVA(format, args);
   va_end(args);

}
void fcopy(FILE *f1, FILE *f2)
{
   char            buffer[BUFSIZ];
   size_t          n;

   while ((n = fread(buffer, sizeof(char), sizeof(buffer), f1)) > 0)
   {
      if (fwrite(buffer, sizeof(char), n, f2) != n)
         outputString("write failed\n");
   }
}


void fileCopy(const char *s, const char *d)
{
   FILE *fp1;
   FILE *fp2;

   if ((fp1 = fopen(s, "rb")) == 0)
      outputString("cannot open file %s for reading\n", s);
   if ((fp2 = fopen(d, "wb")) == 0)
      outputString("cannot open file %s for writing\n", d);
   fcopy(fp1, fp2);
   fclose(fp1);
   fclose(fp2);
}



void logMessage(int level, const char *format, ...)
{
   static int last = 0;
   va_list args;
   if (level > xxLogLevel)
      return;
   outputString("%5d (el:%4d): ", xxCurrentTime / 1000, (xxCurrentTime / 1000) - last);
   last = xxCurrentTime / 1000;
   va_start(args, format);
   outputStringVA(format, args);
   va_end(args);
   outputString("\n");
}
uint32_t millis()
{
   return xxCurrentTime / 1000;
}
void midiSendClock()
{

}


void advanceClock(int increment)
{
   // internalClock is called every n milliseconds
   // where n is the period of time between a 24 ppqn beat
   // (same as midi clock)
   // we need to use microseconds to fix rounding issues
   int period = (1000 * 1000 / (gSeqState.tempo * 24 / 60));
   increment *= 1000; // convert to uS
   while (increment--) {
      // emulate the timer
      if (xxCurrentTime % period == 0) {
         seqClockTick();
      }

      // call main loop
      if ((increment % 1000) == 0)
         seqCheckState();
      xxCurrentTime++;
   }
   outputString("\n");


}

struct Command {
   const char *cmd;
   const char *help;
};

Command cmds[] = {
   ////////////////////////////////////////////////////////////////////////////////]
   { "q","quit" }, //0
   { "cl","<n> advance clock n milliseconds default one" }, //1
   { "div","<num> <denom> set divider num/denom" }, //2
   { "len","<len> set track len (pass 1 based)" }, //3
   { "tempo","<n> set tempo to n" }, //4
   { "show", "<n> show everything. if n is there it shows track n info too" }, //5
   { "track", "<n> set track to <n> (pass 1 based)" }, //6
   { "play",  "<n> where n is 0=stop, 1=play" }, //7
   { "random","<n> generate a number from 0 to n-1" }, //8
   { "mute", "<n> <state> Mute/unmute track n (1 based) state is 0=unmute 1=mute" }, //9
   { "pat", "<n> <d> pattern switch to pattern n=1..4, d=delay switch (1=yes/0=no)" }, //10
   { "save", "<n> save all commands thusfar to file n" }, //11
   { "load", "<n> run commands from file n" }, //12
   { "midi", "<n> set midi note 0..127 for current track" }, //13
   { "step", "<stepnum> <vel> <prob> set vel (0..15), prob (1..16) vel 0=off" }, //14
   { "qstep","<n n n...> Turn steps on/off (0=off)" }, //15
   { "regen","<n> Regenerate random every n steps (1..32)" }, //16
   { "swing","<n> set swing to n (0..100)" }, //17
   { "patprob","<n> <n> <n> <n> Switch probability (n: target pattern, 0-15, 0=never)" }, // 18
   { "patrep","<n> Times to repeat pattern (1..16)" }, // 19
   { "pathold","<n> If 1 will hold the pattern indefinitely" }, // 20   
   { 0,0 }
};

void showCommands()
{
   Command *c = cmds;
   while (c->cmd) {
      outputString("%6s - %s\n", c->cmd, c->help);
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
            // switch back to stdin for reading commands
            fclose(xxInputStream);
            xxInputStream = stdin;
            goto reRead;
         }
      }
   }

   if (xxInputStream != stdin) { // if reading commands from file, dump it out for informational purposes
      outputString("%s", cmdbuf);
   }

   // write it to dump file
   if (xxDumpFile)
   {
      fprintf(xxDumpFile, cmdbuf);
   }

   strcpy(savebuf, cmdbuf);

   memset(params, 0, 100 * sizeof(int));
   pos = cmdbuf;
   word = cmdbuf;
   bool theEnd = false;
   while (!theEnd) {
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
   outputString("[Pat %d %sTrk %d Time %dms]\n"
      "Tempo %d, swing %d (clock every %dms, 16th step every %dms) %s\n",
      gRunningState.pattern + 1,
      gRunningState.patternHold ? "(HOLD) " : "",
      gRunningState.currentTrack + 1,
      xxCurrentTime / 1000,
      (int)gSeqState.tempo,
      gSeqState.swing,
      gSeqState.tempo ? (1000 * 60 / 24) / gSeqState.tempo : 0,
      gSeqState.tempo ? (1000 * 60 / 4) / gSeqState.tempo : 0,
      gRunningState.transportState == TRANSPORT_STARTED ? "started" : "stopped"
   );
   outputString("Regen every %d steps. MIDI channel %d\n", gSeqState.randomRegen, gSeqState.midiChannel);
   if (gRunningState.nextPattern < NUM_PATTERNS) {
      outputString("Next pattern %d (%s)\n", gRunningState.nextPattern + 1,
         gSeqState.delayPatternSwitch ? "delay" : "immediate");
   }

   outputString("Pattern repeats %d. Probabilities: 1=%d 2=%d 3=%d 4=%d\n",
      gSeqState.patterns[gRunningState.pattern].numCycles + 1,
      (gSeqState.patterns[gRunningState.pattern].nextPatternProb[0] & 0xf),
      (gSeqState.patterns[gRunningState.pattern].nextPatternProb[0] >> 4),
      (gSeqState.patterns[gRunningState.pattern].nextPatternProb[1] & 0xf),
      (gSeqState.patterns[gRunningState.pattern].nextPatternProb[1] >> 4));


   for (int i = 0; i < NUM_TRACKS; i++) {
      outputString("Track %d: steps: %d divider(%d/%d) MIDI %d %s\n", i + 1,
         gSeqState.patterns[gRunningState.pattern].tracks[i].numSteps + 1,
         gSeqState.patterns[gRunningState.pattern].tracks[i].clockDividerNum + 1,
         gSeqState.patterns[gRunningState.pattern].tracks[i].clockDividerDenom + 1,
         gSeqState.patterns[gRunningState.pattern].tracks[i].midiNote,
         gSeqState.patterns[gRunningState.pattern].tracks[i].muted ? "muted" : ""
      );
   }

   if (track) {
      int i;
      outputString("CURRENT Track (%d) state (top row velocity bottom row prob)\n", gRunningState.currentTrack + 1);
      SeqTrack *tr = &gSeqState.patterns[gRunningState.pattern].tracks[gRunningState.currentTrack];
      for (int cycle = 0; cycle < 2; cycle++) {
         int start = cycle * 16;
         int len = start + 16;
         if (len > tr->numSteps + 1)
            len = tr->numSteps + 1;
         for (i = start; i < len; i++) {
            outputString("%02d_", i + 1);
         }
         if (start<len)
            outputString("\n");
         for (i = start; i < len; i++) {
            if (tr->steps[i].velocity)
               outputString("%02d ", tr->steps[i].velocity);
            else
               outputString("   ");
         }
         if (start<len)
            outputString("\n");
         for (i = start; i < len; i++) {
            if (tr->steps[i].velocity)
               outputString("%02d ", tr->steps[i].probability + 1);
            else
               outputString("   ");
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
      outputString("Failed to create " DEFAULT_LOG_FILE "\n");
      return;
   }
   xxDumpFile = fopen(SAVE_OUTPUT_FILE, "w");
   if (!xxDumpFile) {
      outputString("Failed to create " SAVE_OUTPUT_FILE "\n");
      return;
   }
   seqSetup();

   rndSRandom(42u, 51u);

   seqSetBPM(120);

   for (;;) {

      statePattern = &gSeqState.patterns[gRunningState.pattern];
      stateTrack = &statePattern->tracks[gRunningState.currentTrack];
      runTrack = &gRunningState.trackPositions[gRunningState.currentTrack];

      outputString("%dms >", xxCurrentTime / 1000);
      c = getCommand(params, &pc);
      switch (c) {
      case -1:
         outputString("Available commands:\n");
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
            outputString("unsupported clock divider\n");
         }
         else {
            stateTrack->clockDividerNum = num - 1;
            stateTrack->clockDividerDenom = denom - 1;
         }

      }
      break;
      case 3: // len
         if (params[0] > 32 || params[0] < 1) {
            outputString("Invalid len (1-32)\n");
         }
         else {
            stateTrack->numSteps = params[0] - 1;
         }

         break;
      case 4: // tempo
         if (params[0] > 255 || params[0] < 1)
            outputString("Invalid tempo (1-255)\n");
         else
            seqSetBPM(params[0]);
         break;
      case 5: // show everything
         showState(params[0] != 0);
         break;
      case 6: // set current track (1 based)
         if (params[0] > 0 && params[0] < 7) {
            gRunningState.currentTrack = params[0] - 1;
            outputString("Track set to %d\n", params[0]);
         }
         else {
            outputString("Invalid track\n");
         }
         break;
      case 7: // transport 0=stop, 1=play
         if (params[0] == TRANSPORT_STOPPED || params[0] == TRANSPORT_STARTED)
            seqSetTransportState(params[0]);
         else
            outputString("Invalid state [0 stop, 1 start]\n");
         break;
      case 8: // random
         outputString("random number from 0 to %d inclusive is: %d\n", params[0] - 1, rndRandom(params[0]));
         break;
      case 9: // mute a track
         if (params[0] > 0 && params[0] < 7 && params[1] >= 0 && params[1] <= 1) {
            gSeqState.patterns[gRunningState.pattern].tracks[params[0] - 1].muted =
               params[1];
            outputString("Track %d %s\n", params[0], params[1] ? "muted" : "unmuted");
         }
         else {
            outputString("Invalid track\n");
         }
         break;
      case 10: // switch pat
         if (params[0] < 1 || params[0] > NUM_PATTERNS ||
            params[1] < 0 || params[1] > 1)
            outputString("Invalid pattern or delay setting\n");
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
            outputString("File open failed for %s\n", fnbuf);
         }
         break;
      }
      case 13: // set midi note for track
         if (params[0] >= 0 && params[0] < 128)
            stateTrack->midiNote = params[0];
         else
            outputString("You dullard! Enter a proper range\n");
         break;
      case 14: // step vel prob 0..15 1..16
         if (params[0] >= 1 && params[0] <= NUM_STEPS &&
            params[1] >= 0 && params[1] < 16 &&
            params[2] >= 1 && params[2] <= 16) {

            stateTrack->steps[params[0] - 1].velocity = params[1];
            stateTrack->steps[params[0] - 1].probability = params[2] - 1;
         }
         else {
            outputString("Out of range for one of those. Try again\n");
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
            outputString("Range is 1..32\n");
         break;
      case 17: // swing
         if (params[0] >= 0 && params[0] <= 100)
            gSeqState.swing = params[0];
         else
            outputString("Range is 0..100\n");
         break;
      case 18: // pattern probability 0..15
         for (int pp = 0; pp < 4; pp++) {
            int idx = pp / 2;
            if (params[pp] < 0 || params[pp] > 15) {
               outputString("Range is 0..15. Pattern %d not changed\n", pp + 1);
            }
            else {
               if (pp % 2 == 0) { // low nibble
                  statePattern->nextPatternProb[idx] &= 0xF0; // clear low nibble
                  statePattern->nextPatternProb[idx] |= params[pp]; // set it
               }
               else { // high nibble
                  statePattern->nextPatternProb[idx] &= 0x0F; // clear high nibble
                  statePattern->nextPatternProb[idx] |= (params[pp] << 4); // set it
               }
            }
         }
         break;
      case 19: // pattern num repeats 1..16
         if (params[0] < 1 || params[0] > 16) {
            outputString("Invalid number of repeats (1..16)\n");
         }
         else {
            statePattern->numCycles = params[0] - 1;
         }

         break;
      case 20: // pattern hold on or off
         if (params[0] > 0)
            gRunningState.patternHold = 1;
         else
            gRunningState.patternHold = 0;
         break;

      } // end case
   } // end for(;;)

end:
   if (xxLog)
      fclose(xxLog);
   if (xxDumpFile)
      fclose(xxDumpFile);

}


int main(int argc, char *argv[])
{
   doSequencer();
   //while(doCommand());

}

void clockSetBPM(uint8_t) {}
void midiSendStart(void) {}
void midiSendStop(void) {}
void midiPlayNote(uint8_t, uint8_t, uint8_t) {}