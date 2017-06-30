#include "main.h"
#define BUTTON_BYTES 5
#define DEBOUNCE_TIME 5
bool hwButtons[40] = {};

unsigned int gMillis = 0;
static unsigned int millis() {
   return gMillis;
}
 
uint8_t shiftIn(int index)
{
   uint8_t result = 0;
   uint8_t mask;
   for (int i = 0; i<8; i++) {
      mask = (((hwButtons[(index * 8) + i]) ? 1 : 0) << i);
      result |= mask;
   }
   return result;
}


//------------------------------------------------------
uint8_t gButtonJustChanged[BUTTON_BYTES];
uint8_t gButtonIsPressed[BUTTON_BYTES];

#define BUTTON_IS_PRESSED(index) \
  ((gButtonIsPressed[(index)/8]) & (1 << ((index) % 8)))

#define BUTTON_JUST_CHANGED(index) \
  ((gButtonJustChanged[(index)/8]) & (1 << ((index) % 8)))

#define BUTTON_JUST_PRESSED(index) \
  (BUTTON_JUST_CHANGED(index) && BUTTON_IS_PRESSED(index))  

#define BUTTON_JUST_RELEASED(index) \
  (BUTTON_JUST_CHANGED(index) && (!BUTTON_IS_PRESSED(index)))  



void check_switches()
{
   static uint8_t previousBtnState[BUTTON_BYTES]; // one bit for each
   static uint8_t currentBtnState[BUTTON_BYTES]; // one bit for each
   static uint8_t debounceLastTime;
   uint8_t index, mask, btnByte, currentState;


   // debounce. No need to take up more than a byte which gives us a range of 255 milliseconds 
   // we will definitely be in this function in much shorter periods than that.
   if ((int8_t)((uint8_t)millis() - debounceLastTime) < DEBOUNCE_TIME)
      return; // not enough time has passed to debounce  
   debounceLastTime = (uint8_t)millis();


   //BUTTONS
   //while the shift register is in serial mode
   //collect each shift register into a byte
   //the register attached to the chip comes in first 
   for (index = 0; index < BUTTON_BYTES; index++)
      currentBtnState[index] = shiftIn(index);

   /*
   //write the button states to the currentstate array
   for (byte j = 0; j < BUTTON_BYTES; j++)
   {
   for (byte i = 0; i<=7; i++)
   currentstate[(j * 8) + i] = BUTTONvar[j] & (1 << i);
   }
   */
   // when we start, we clear out the "just" indicators
   memset(gButtonJustChanged, 0, BUTTON_BYTES);

   for (index = 0; index < 40; index++) {
      btnByte = index / 8;        // which byte to address in each array
      mask = 1 << (index % 8);  // used to extract the bit we want
      currentState = currentBtnState[btnByte] & mask;
      if (currentState == (previousBtnState[btnByte] & mask)) { // debounce state stable
                                                                // if it's not what it was, say that it's just changed
         if ((gButtonIsPressed[btnByte] & mask) != currentState)
            gButtonJustChanged[btnByte] |= mask; // set the bit to 1

                                                 // this allows us to check whether the button is down
         gButtonIsPressed[btnByte] &= (~mask); // turn the bit off      
         gButtonIsPressed[btnByte] |= currentState; // set to match current state

      }
      else {
         // save the current state since it differs from previous
         previousBtnState[btnByte] &= (~mask); // turn bit off
         previousBtnState[btnByte] |= currentState; // set bit to match current state
      }
   }
}


//------------------------------------------------------------
int doCommand()
{
   int cnt;
   char cmdbuf[2];

   printf("Time is %d\ncmds\nSet btn[s x]\nclear btn [c x]\nshow values [d 0]\nAdvance timer (ms) [a x]\n->", gMillis);
   scanf("%s %d", cmdbuf, &cnt);
   switch (cmdbuf[0]) {
   case 's':
      hwButtons[cnt] = 1;
      break;
   case 'c':
      hwButtons[cnt] = 0;
      break;
   case 'd':
      printf("Actual  Pressed Changed JustPre JustRel\n");
      for (int i = 0; i<40; i++) {
         printf("%07d %07d %07d %07d %07d\n",
            hwButtons[i],
            BUTTON_IS_PRESSED(i),
            BUTTON_JUST_CHANGED(i),
            BUTTON_JUST_PRESSED(i),
            BUTTON_JUST_RELEASED(i));
      }
      break;
   case 'a':
      gMillis += cnt;
      check_switches();
      break;
   default:
      return 0;
   }
   return 1;

}
