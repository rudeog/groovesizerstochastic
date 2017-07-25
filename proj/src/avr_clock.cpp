#include "avr_main.h"

/* seqClockTick gets called when timer1 ticks. This corresponds to BPM so it
*  will get called 24 times per quarter note.
*  It will not be called at all when we are in midi slave mode
*/

// things that need to happen as a consequence of the bpm changing
void clockSetBPM(uint8_t BPM)
{   
   if(BPM ==0 ) { // bpm of 0 indicates ext clock sync    
      Timer1.detachInterrupt(); // remove callback fn if any
   } else {
      // use 24 ticks per beat - same as midi clock
      // TODO check for big rounding, or intermediate overflow
      Timer1.setPeriod(1000000/(BPM*24/60)); // with period in microseconds (same as 1000000*60/BPM/24)
      Timer1.attachInterrupt(seqClockTick);
   }
}

void
clockSetup(void)
{
  // setup for the sequencer timer (default timer interval is 1000ms)
  Timer1.initialize(0);  
}

