A synthesizer with six sounds. You mix them together. Then you get a lotta sounds.

Synth options:

-Oscillator types: Sine, Triangle, Saw, Pulse, Additive.

-Filter types: Lowpass, Highpass, Bipole, Comb.

-Three simple, global-only effects; Delay, Reverb, Chorus

-You can sync an oscillator to DAW tempo and use it as an LFO. Do you want more LFOs? Why have LFOs when you can just have more oscillators?

-You can pass audio into an oscillator with the audio routing matrix too, so you can use an oscillator as a filter. Do you want more filters? Why have more filters when you can just have oscillators?

-You can modulate any oscillator with any other oscillator. You can also modulate specific settings with other oscillators. Three of them. If you want more, no you don't. I don't want to think about it too hard.

-Preset support.

-Incredibly simple randomization.

-An Init button to avoid the randomization when it becomes really too much (immediately).

-A gain function to attempt to not deafen everyone who uses this synth (optimistic).

-A soft clipper on the end of the chain and at every step of the audio routing page, which seems to have actually solved the deafening problem (still optimistic though).

-if you want to sync any of the effect sounds to anything else ... use an external effect, thank you. i just wanted to avoid /needing/ an effect for a few simple JUCE boilerplate things.

Pictures of the synth in use are provided in the pictures/ folder. Builds are provided in the binaries/ folder.


This is almost entirely AI-coded, but I did yell at the AI a fair bit, and interrupted/guided where I thought it was going wrong, pulled out a few variables ... I ripped out the limiter myself and coded up most of the gain function because I was mad about my speakers being unhappy .. etc. ... Also I wrote all of the text that isn't inside code blocks.
It builds on my machine on Windows, MacOS, and Linux.
