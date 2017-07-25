#ifndef AVR_STRUCTS_H_
#define AVR_STRUCTS_H_
// AVR only file that defines hardware stuff
#ifdef _WIN32
#error AVR specific code should not be included
#endif

////////////////////////
// BUTTONS
// 5 x 8 = 40 --- do the math
#define BUTTON_BYTES 5

#define BUTTON_IS_PRESSED(index) \
  ((gButtonIsPressed[(index)/8]) & (1 << ((index) % 8)))

#define BUTTON_JUST_CHANGED(index) \
  ((gButtonJustChanged[(index)/8]) & (1 << ((index) % 8)))

#define BUTTON_JUST_PRESSED(index) \
  (BUTTON_JUST_CHANGED(index) && BUTTON_IS_PRESSED(index))  
  
#define BUTTON_JUST_RELEASED(index) \
  (BUTTON_JUST_CHANGED(index) && (!BUTTON_IS_PRESSED(index)))    

extern uint8_t gButtonJustChanged[BUTTON_BYTES];
extern uint8_t gButtonIsPressed[BUTTON_BYTES];
  
  
//****POTS ***************************************************************************************
#define POT_COUNT 6
#define POT_1     0
#define POT_2     1
#define POT_3     2
#define POT_4     3
#define POT_5     4
#define POT_6     5


// determines whether a pot's value has just changed
#define POT_JUST_CHANGED(index) \
  (gPotChangeFlag & (1 << (index)))
// use to get the value of the pot  
#define POT_VALUE(index) gPotValue[(index)]  

extern uint16_t gPotValue[POT_COUNT];  // to store the values of out 6 pots (range 0-1023)
extern uint8_t gPotChangeFlag;         // determine whether pot has changed

#endif
