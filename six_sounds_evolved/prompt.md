Hello Gemini, I would like to make a four-operator VST synthesizer using the JUCE framework. The synthesizer should have:
-A top panel.
-On the left of the top panel, there should be a small label showing which preset you have selected. When you open the synth, it should start by having an 'Init' preset. The Init preset is set to just play a simple Sine wave with nothing else selected.
-In the middle, there should be an oscilliscope, showing whatever you're playing in the synth at the time.
-On the right, there should be a Master Volume for the synth, from 0 to 100.
-A panel right under that top panel. Within that panel, there should be two 'pages' in the UI. This will have two very long clickable buttons that control the graphics underneath them. One selection (all the way on the left) should be 'Oscillators', and one all the way on the right should be 'Modulation'.

Let's detail the Oscillators page now.
 
We want this page to have four 'oscillators' in a row, going down:
1
2
3
4

Each oscillator should have controls like this:

1 Wave: [Sound wave] Type: [Sine] Pulsewidth: [50]      | Attack: [0] Decay: [0] Sustain: [0] Release: [0]    | Output: [100]

-The Wave options are: [Sound wave, Additive wave, Resonator, Filter] Default Sound Wave
-Each of these oscillator types should have different selections. The Wave options should dynamically resize if you select a different option.

-The controls for the oscillator should change depending on the type of oscillator that the user has selected.
If a sound wave is selected, the options should be:
Type: [Sine, Saw, Pulse, Square] Default is Sine
Ratio: [from 0.00 to 16.00] Defalut is 1, and you should be able to select any option between 0.00 and 16.00. This lets you set up ratios between operators.
Pulsewidth: [0-100] Default is 50
If an additive wave is selected, the options should be:
Partial 1: [1,2,3,4,5,6,7,8] Default 1
Partial 2: [1,2,3,4,5,6,7,8] Default 1
Partial 3: [1,2,3,4,5,6,7,8] Default 1
Partial 4: [1,2,3,4,5,6,7,8] Default 1
Partial 5: [1,2,3,4,5,6,7,8] Default 1
Partial 6: [1,2,3,4,5,6,7,8] Default 1
Partial 7: [1,2,3,4,5,6,7,8] Default 1
Partial 8: [1,2,3,4,5,6,7,8] Default 1
This will function sort of like an 8-bit wave as it should move the individual parts of the sound wave the oscillator uses in blocks.

If a resonator is selected, the options should be:
Key: [A, A#, B, B#, C, C#, D, D#, E, E#, F, F#, G, G#] Default C
Q: 0-100, controls how wide the resonator is. Default 50
Damping, 0-100, controls how strong the resonation is. Default 50

If a filter is selected, the options should be:
Type: [ LP12, LP24, HP12, HP24 ]
Resonance [0-100] Default 0
Cutoff [0-100] Default 100

For the 'Modulation' page that shows if you select the right part of the middle bar labeled 'Modulation', we want:
-A 4-by-4 grid letting you select frequency modulation for each oscillator. This is the amount that any one oscillator modulates another oscillator. So Oscillator 1 should be able to modulate 12,3, and 4, and so on, like this:

1 | 1 2 3 4
2 | 1 2 3 4
3 | 1 2 3 4
4 | 1 2 3 4
 
-Under that, there should be two low-frequency oscillators (LFOs) that you can use to modulate anything else in the synth you want, using a dropdown menu. This should have the options of Target, Rate [in speed] and Type [the same options as the Type from the Sound Wave options above].

Please give me code that, when run, will give me a VST synth that performs to these specifications.
