void setup() 
{
  // set initial sequencer state (only sets some global vars)
  seqSetup();
  // below may rely on sequencer state
  ledsSetup();
  buttonSetup();
  potsSetup();
  clockSetup();
  midiSetup();  
  eepromSetup();
  
  // read current pot values now so that when we read them in main loop they will
  // not be perceived to change and therefore not flag just-changed indicators
  potsUpdate();

  // boot message - show version
  ledsShowNumber(VERSION_NUMBER);
  
}

