#include "avr_main.h"
#define POTS_AVG_COUNT 4
#define HYSTERESIS     1
#define ANALOGPIN      0      // the analog pin connected to multiplexer output
/* @AS read the pot values and save a bit as to whether they've changed
 */ 
void
potsSetup(void)
{
  // set the three 4051 select pins to output
  // the pins are wired to the multiplexer select bit and are used to select 1 of 8 inputs on multiplexer
  pinMode(17,OUTPUT);
  pinMode(16,OUTPUT);
  pinMode(15,OUTPUT);  
}

// POTS reading from 4051
// this function returns the analog value for the given channel
static uint16_t
getPotValue(uint8_t channel)
{
  // set the selector pins HIGH and LOW to match the binary value of channel
  digitalWrite(17, bitRead(channel, 0));
  digitalWrite(16, bitRead(channel, 1));
  digitalWrite(15, bitRead(channel, 2));  
  // we're only using readings between 25 and 1000, as the extreme ends are unstable
  // THIS IS NEEDED! otherwise problems will happen
  return map((constrain(analogRead(ANALOGPIN), 25, 1000)), 25, 1000, 0, 1023); 
  //return analogRead(ANALOGPIN);
  
}

// determine the current value of the pots
// (called from main loop)
void
potsUpdate(void)
{
   uint8_t ctr;
   uint16_t val, sum;
   gPotChangeFlag=0;
   for(uint8_t i=0;i<POT_COUNT;i++) {// each pot
   
      // get an average of 4 readings
      // THIS IS ALSO NEEDED!
      sum=0;
      for(uint8_t ctr=0; ctr<POTS_AVG_COUNT; ctr++)
         sum+=getPotValue(i);
      val=sum/POTS_AVG_COUNT;
      
   
      //val = getPotValue(i);
      
      // don't want underflows here when comparing
      // HYSTERESIS value was taken from LXR code where it seems fine. Original code here had 35 as the threshold for
      // indicating a changed value
      if ((val > ( gPotValue[i] + HYSTERESIS)) ||
      (gPotValue[i] > (val + HYSTERESIS))) {
         gPotChangeFlag |= (1 << i); // indicate that it's value has changed
         gPotValue[i]=val;
      }
      
   }
}

