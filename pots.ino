/* @AS the idea here is to be able to determine when a pot has changed
 *  so we can react to it. we lock all the pots to begin with whenever mode changes (and whenever one changes we lock the others)
 *  and then when pot value changes, it differs from it's locked value so we know it's changed
 *  and we can react to that.
 */
void lockPot(byte Pot) // values of 0 - 5 for each pot and 6 for all
{
  if (Pot == 6)
  {
    for (byte i = 0; i < 6; i++)
      potLock[i] = pot[i];
  }
  else
    potLock[Pot] = pot[Pot];
  showingNumbers = false;
  lockTimer = 0;
}

static inline int 
difference(int i, int j)
{
  int k = i - j;
  if (k < 0)
    k = j - i;
  return k;
}


boolean unlockedPot(byte Pot) // check if a pot is locked or not
{
  if (potLock[Pot] == 9999)
    return true;
  else if (difference(potLock[Pot], pot[Pot]) > 35) // can lower the threshold value (35) for less jittery pots
  {
    for (byte i = 0; i < 6; i++)// lock all the other pots
    {
      if (i != Pot)
        lockPot(i);
    }
    showingNumbers = true;
    potLock[Pot] = 9999;
    return true;
  }
  else
    return false;
}

// POTS
// reading from 4051
// this function returns the analog value for the given channel
static int
getPotValue( int channel)
{
  // set the selector pins HIGH and LOW to match the binary value of channel
  for(int bit = 0; bit < 3; bit++)
  {
    int pin = select[bit]; // the pin wired to the multiplexer select bit
    int isBitSet =  bitRead(channel, bit); // true if given bit set in channel
    digitalWrite(pin, isBitSet);
  }
  return map((constrain(analogRead(analogPin), 25, 1000)), 25, 1000, 0, 1023); // we're only using readings between 25 and 1000, as the extreme ends are unstable
  //int reading = analogRead(analogPin);
  //return reading;
}


// determine the current value of the pots
// (called from main loop)
void potsUpdate()
{
  // we want to get a running average to smooth the pot values
  static int potValues[6][10] ; // to store our raw pot readings
  static byte index;
  for (byte i = 0; i < 6; i++) // for each of the 6 pots
    potValues[i][index] = getPotValue(i); 
  index = (index < 9) ? index + 1 : 0;  
  for (byte i = 0; i < 6; i++)
  {
    int tempVal = 0;
    for (byte j = 0; j < 10; j++)
    {
      tempVal += potValues[i][j];
    }
    pot[i] = tempVal / 10; // get the average
  }
}


#if 0
/* There are four pots on the lxr
 * adcPots.c
 *
 * Created: 27.04.2012 16:15:14
 *  Author: Julian
 */ 
#include "adcPots.h"
#include <string.h>
#include "../Menu/menu.h"

#define HYSTERSIS     3
#define MIN( a, b ) (a < b) ? a : b


uint16_t adc_potValues[4];
uint16_t adc_potAvg[4];
//------------------------------------------------------------------------
void adc_init(void) {
 
  memset((void*)adc_potValues,0,8);
  memset((void*)adc_potAvg,0,8);
  uint16_t result;
 
  // AVCC as ref voltage
  ADMUX =  (1<<REFS0);
  
  // single conversion
  ADCSRA = (1<<ADPS2) |(1<<ADPS1) /*| (1<<ADPS0)*/;     // adc prescaler div 64 => 8mhz clock = 125 kHz (must be between 50 and 200kHz)
  ADCSRA |= (1<<ADEN);                  // ADC enable
 
  // dummy readout
  ADCSRA |= (1<<ADSC);                  // single readout
  while (ADCSRA & (1<<ADSC) ) {}        // wait to finish
  // read result 
  result = ADCW;
}
//------------------------------------------------------------------------
uint16_t adc_read( uint8_t channel )
{
  //select channel
  ADMUX = (ADMUX & ~(0x1F)) | (channel & 0x1F);
  ADCSRA |= (1<<ADSC);            // single readout
  while (ADCSRA & (1<<ADSC) ) {}  // wait to finish
  return ADCW;                    // read and return result
}
 //------------------------------------------------------------------------
uint16_t adc_readAvg( uint8_t channel, uint8_t nsamples )
{
  uint32_t sum = 0;
 
  for (uint8_t i = 0; i < nsamples; ++i ) {
    sum += adc_read( channel );
  }
  
  return (uint16_t)( sum / nsamples );
}
//------------------------------------------------------------------------ 
uint8_t adcCnt = 0;
void adc_checkPots()
{
  //adc_potAvg
  
  //if((adcCnt++)&0x07 == 0x00)
  {
    for(int i=0;i<4;i++)
    { 
    
      uint16_t newValue = adc_readAvg(i,4);//adc_read(i);
    
       if ((newValue > ( adc_potValues[i] + HYSTERSIS)) ||
         ( adc_potValues[i] > (newValue + HYSTERSIS)))
       {
         adc_potValues[i] = newValue;
         menu_parseKnobValue(i,(newValue>>2));
         menu_repaintAll();
       } 
    }
  }   
};
//------------------------------------------------------------------------
#endif
