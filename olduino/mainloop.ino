// Main loop
void loop() 
{
  
  // This will invoke callbacks - see HandleMidi   
  midiUpdate();
  
  
  // see if any buttons are pressed. If they were just pressed, the just indicators will be set
  // and can be checked below this line (they are reset every time this is called)
  buttonsUpdate();

  // get the pot values (also updates just-twiddled)
  potsUpdate();

  // respond to user. 
  uiAcceptInput();
  
  // do sequencer - advance tracks, patterns, etc.
  // this may trigger midi notes
  seqCheckState();
  
  // update screen to reflect reality
  ledsUpdate();
  
}





