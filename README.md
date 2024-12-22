---
author: Andrew Shakinovsky
operator: Andrew Shakinovsky
---

# Groovesize Stochastic

# Tips

When powering up make sure pots 1 and 5 are at 0 or it will not go past
the intro mode.

Otherwise, twist a knob or push a button to generate a random seed, and
it will go past the intro mode.

Setting tempo to 0 puts the device in slave mode, in which case it will
need to receive midi start/stop and midi sync to play.

Track 1 of a pattern determines the length of a cycle for the purpose of
switching patterns. If a pattern will switch after 1 cycle, it is the
length of track 1 that determines this.

# Architecture

The main entity you work with is a patch. There are 16 locations to
store 16 patches in eeprom.

Each patch consists of four patterns, and 6 tracks.

Each pattern can contain up to 32 steps on each track. Each track can
have settings independent of other tracks which allow for polyrhythms to
be created (for example a 3-4 rhythm against a 4-4 rhythm). Note that
these settings are shared between the patterns, so for example the track
settings on pattern 1 will be the same for pattern 2..4. Think of it as
a grid with track data on one axis and pattern data on the other axis,
and the actual sequence data in the grid itself.

## Track data:

- Midi channel -- which midi channel will receive the triggered notes

- Midi note -- which midi note number will be triggered

- Clock divider (1..4 over 1..4) -- determines playback speed for the
  track. 1/1 is normal speed.

- Number of steps (1..32) determines how many steps are in the track

- Step length. The length of the midi note in 16^th^ measures (max 2
  measures length)

- Mute/Unmute state

## Pattern data:

- Number of cycles to play the pattern (track 1 of the pattern
  determines a cycle length)

- Probability of switching to pattern 1..4 after pattern has played it's
  cycles. Lowest probability means never switch.

## Sequence data:

- Each step in the pattern has a velocity and probability of playing for
  each track

## Patch data:

- Tempo. If set to 0, sets slave mode

- Swing

- Delay pattern switch mode -- if on and user manually switches
  patterns, will wait till end of cycle to switch

- Random regen -- regenerate a new random number every n steps. This
  allows you to have a series of steps with the same probability always
  do the same thing as each other.

- Transmit clock -- whether to transmit a clock signal on midi out

- Midi Thru -- whether to send midi input data to midi output

# Basic Usage

The device has the following controls:

- Knobs -- these are the knobs at the top numbered 1 to 6. Knob 1 and 2
  are used to change track parameters. Knobs 5 and 6 are used to change
  patch parameters. Knob 3 is used to change tempo

- Step buttons -- these are the 32 main buttons in the center. They are
  mainly used to toggle steps, but have other functions as well

- Function buttons -- these are the buttons labeled F1 to F6. They are
  used to select or mute tracks, select patterns.

- Shift buttons -- there is the left shift and the right shift buttons

To get started, for each track, select which midi note and channel will
be used. These should match the channel and note on the drum machine.
Toggle steps using the main grid buttons. Play the sequencer by holding
one of the shift buttons and pressing the other shift button.

## Main Mode

This is the mode that is active when no shift buttons are held, and
knobs 1 and 5 are all the way to the left.

- Pressing step buttons will toggle them on or off. When toggled on, the
  velocity and probability is set to max

- Use the function buttons to switch between the 6 tracks

- Hold Left shift and press a function button to mute/unmute that track

- Turn knob 3 to change tempo. Set tempo to 0 to slave to midi clock

- Hold left shift and touch knob 3 to show current tempo

- Hold left shift and touch knob 4 to show actual running tempo (ie if
  slaved will show the tempo being received)

- Turn knobs 1 or 5 to the right to enter knob edit mode (see below)

- Press left shift and right shift together to start or stop playback

## Knob Edit Mode

- Turn knob 1 to edit track parameters. As you turn it, lights on row 1
  of the step buttons will indicate which parameter is being edited
  (channel, note number, muted, clock numerator, clock denominator,
  number of steps, step length). Turn knob 2 to change the value of the
  parameter. If you hold left shift while turning knob 2 it's current
  value will be displayed and will not be changed.

- Turn knob 5 to edit patch parameters. As you turn it the lights on row
  2 of the step buttons will indicate which parameter is being edited
  (Swing, Random Regen, Delay Pattern Switch, Send Clock, MIDI thru).
  Turn knob 6 to change these values. If you hold left shift while
  turning knob 6 it's current value will be displayed and will not be
  changed.

- Turn knobs 1 and 5 back to the left to return to main mode

- When in knob edit mode if you are editing a yes/no value, the top
  right light will indicate yes or no (yes=on). Also the top right
  button can be used to toggle yes/no

## Step Edit Mode

This mode is used to edit the details of an individual step. Hold the
left shift button and select a step to edit. In this mode the top two
rows of the step buttons set the velocity for the step, and the bottom
two rows set the probability. Select a new velocity or probability, or
press left shift button to return to main mode.

## Pattern Edit/Other Mode

This mode is used to determine when patterns will change, as well as to
load/save patches. It is entered by holding the right shift button, and
while holding it, press one or more other buttons. When the right shift
button is released, you are returned to main mode.

- In this mode, pressing one of the F1..F4 buttons will switch between
  patterns. If Delay Pattern Switch mode is on, and the sequencer is
  playing, then the switch will only occur at the end of the cycle.
  Otherwise it will happen immediately.

- Pressing one of the first four buttons on the bottom row of the step
  buttons will allow you to determine the likelihood of switching to
  that pattern (1..4) when the current pattern is done. If you press one
  of these, the top two rows of green led's will indicate the likelihood
  of switching to that pattern. Pressing the corresponding buttons will
  set the likelihood. Pressing the first one is equivalent to "never"

- Pressing the 5^th^ button on the bottom row of step buttons will let
  you select the number of cycles that the current pattern will play
  for. Use the step buttons as above to set the number of cycles

- Press the 7^th^ button on the bottom row to load a new patch. The top
  two rows of step buttons select the patch to load

- Press the 8^th^ button on the bottom row to save the patch. The top
  two rows of the step buttons select the slot to save the patch in. ALL
  data is saved with the patch

- F6 toggles pattern hold. When pattern hold is on, and sequencer is
  playing, the pattern will never switch. This allows you to edit
  pattern data without worrying about it switching to a different
  pattern while you are working.
