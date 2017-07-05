#include "main.h"

RunningState gRunningState;
SequencerState gSeqState;

// use this to figure out size at compile time (currently 835)
//char(*__kaboom)[sizeof(gSeqState)] = 1;
// currently 40
//char(*__kaboom2)[sizeof(gRunningState)] = 1;



int main(int argc, char *argv[])
{
   doSequencer();
   //while(doCommand());

}