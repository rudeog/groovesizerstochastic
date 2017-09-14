#include "avr_main.h"

// size of each page
#define EPBUFSIZE 64
EEPROM256_512 mem_1;            // define the eeprom chip
uint8_t rwBuffer[EPBUFSIZE];           // our EEPROM read/write buffer

void
eepromSetup(void)
{
    //begin I2C Bus
  Wire.begin();
  //begin EEPROM with I2C Address 
  mem_1.begin(0,0);//addr 0 (DEC) type 0 (defined as 24LC256)
}

/* Load data from an eeprom slot
 */
void
eepromLoad(uint8_t slot)
{
   // gSeqState occupies about 804 bytes at the time of this writing
   // that means a bit less than 13 eeprom pages to store all the data
   // (we'll say 16 to be safe)
   // that gives us potentially 36 locations to store data. way more than we need
   // there are 512 eeprom pages of 64 bytes each
   uint8_t page;
   uint16_t bytesRemaining;
   uint8_t len;
   char *pos=(char*)&gSeqState;
   // figure out which page to start reading
   page=slot*16;
   // total bytes to read
   bytesRemaining=sizeof(SequencerState);
   
   while(bytesRemaining) {
      mem_1.readPage(page,rwBuffer);
      // how many bytes of the page do we need
      if(bytesRemaining >= EPBUFSIZE)
         len=EPBUFSIZE; // the whole page
      else
         len=(uint8_t)bytesRemaining; // the remainder
      
      memcpy(pos,rwBuffer,len);
      bytesRemaining -= len;
      pos += len;      
      page++;
   }
   
   // reset running state to safe values
   seqResetRunningState();
   
   // update running state and clock with tempo info
   seqSetBPM(gSeqState.tempo);
   
   // update midi subsystem with current settings
   midiSetThru();
}

/* Save data to eeprom
*/
void
eepromSave(uint8_t slot)
{
   uint8_t page;
   uint16_t bytesRemaining;
   uint8_t len;
   char *pos=(char*)&gSeqState;
   // figure out which page to start writing to
   page=slot*16;
   // total bytes to write
   bytesRemaining=sizeof(SequencerState);
   
   while(bytesRemaining) {      
      // how many bytes of the page do we need
      if(bytesRemaining >= EPBUFSIZE)
         len=EPBUFSIZE; // the whole page
      else
         len=(uint8_t)bytesRemaining; // the remainder
      
      memcpy(rwBuffer, pos, len);
      mem_1.writePage(page,rwBuffer);
      bytesRemaining -= len;
      pos += len; 
      page++;
   }
}
