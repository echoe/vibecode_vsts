A synthesizer with six sounds. You mix them together. Then you get a lotta sounds.

Synth features:

-Different oscillator types!
Sine, Triangle, Saw, Pulse, Additive (it's just a wave, you can't change the wave with drawbars. I kind of want to do that. It feels outside of the mission statement though).
You can change the Ratio, detune the wave, change the phase of the wave, and change the gain of each oscillator (that's how you control whether or not the oscillator makes sound, which is important if you route a bunch of oscillators into other oscillators!).

-Filter types: Lowpass, Highpass, Bipole, Comb.
--You can change the cufoff, resonance, and q factor of each filter.

-You can sync an oscillator to DAW tempo and use it as an LFO. Do you want more LFOs? Why have LFOs when you can just have more oscillators?

-You can pass audio into an oscillator with the audio routing matrix too, so you can use an oscillator as a filter. Do you want more filters? Why have more filters when you can just have oscillators?

-You can modulate any oscillator with any other oscillator. You can also modulate specific settings with other oscillators. Three of them. If you want more lanes, no you don't.

-Three simple, global-only effects to soften up the FM; Delay, Reverb, and Chorus.

-Preset support, in the form of writing xml files, so how much support is it really?

-Randomization of each page individually.

-An Init button to avoid the randomization when it becomes too much.

-A gain function to attempt to not deafen everyone who uses this synth (optimistic).

-A soft clipper on the end of the chain and at every step of the audio routing page, which seems to have actually solved the deafening problem (still optimistic though).

Pictures of the synth in use are provided in the pictures/ folder, if you are curious. Builds are provided in the binaries/ folder.

-If you want to sync any of the effect sounds to anything else ... use an external effect, thank you. i just wanted to avoid /needing/ an effect for a few simple JUCE boilerplate things.

This is almost entirely AI-coded, but I did yell at the AI a fair bit, and interrupted/guided where I thought it was going wrong, pulled out a few variables ... etc. etc.

... I also ripped out the limiter myself and coded up most of the gain function because I was mad about my speakers being unhappy, so I guess that's not really AI. ... Also I wrote all of the text that isn't inside code blocks.
It builds on my machine on Windows, MacOS, and Linux.
Binaries available here: https://github.com/echoe/vibecode_vsts/releases
