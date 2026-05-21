# Two days of vibecoding an FM synth ...

I wanted something that was like the Opsix, and like Six Sines, but was:

- Much simpler than the Opsix VST
- Had filters, like the Opsix, and not like Six Sines. Especially comb filters, they're great, but I also like Low-Pass Filters.
- Works on Linux natively!

Because of that, I eventually got this synth out of the AI, with a lot of testing and checking through the process. I also (despite my best efforts) did learn a little bit about how JUCE functions.

It looks like this:
![alt text](https://github.com/echoe/six_sounds/blob/main/picture/six_sounds_002.png?raw=true)


Features:

*The synth defaults to every 'OUT' switch being off. This means that to hear the default patch (a single sine wave) you will have to raise the 'OUT' slider on the far right from the first oscillator!!*

The sound from each oscillator routes sequentially down to the next. The volume sliders at the end of each mod matrix row will let that oscillator be heard without any filtering from the next oscillators in the chain - in this way, the synth can both process sound sequentially and in parallel.

The context knobs don't re-title themselves right now. They are:

- Sine: C1 is Phase. There are no other options.
- LoPass: C1 is Cutoff, and C2 is Resonance. TRK controls the amount of key tracking.
- Comb: C1 is Feedback, and C2 is delay direction (fastforward or back). TRK controls the amount of key tracking.

There are two envelopes, and you can select between Env1 and Env2 for the oscillators, so you can have some different envelope things. Not as many as Dexed. Your Mileage May, in fact, Vary.

There are also two LFOs, which have ... I guess either eight or ten total destination options, I'm honestly not sure. You can also set up the Mod Wheel in that matrix if you want. I haven't tested that! Weee.

Disclaimers:
-This is the result of two days of doing this (it's actually the first time I have vibecoded anything for any reason). I also had to hand write some of the visualization code because the robot wouldn't do it right, so ... yeah.
-It seems like it takes up a lot of CPU power. Fair warning!
-I am not sure that everything works as shown. But it /appears/ to. Or it did at one point!! Yeah.

To install:
-Drop the vst3 folder into wherever your vst3 folder is. Ezxcept on Mac, where it provides a file instead of a folder, for reasons I am not privy to.

To build:

Linux:
Make sure you have all required things to run JUCE on your machine; put the JUCE folder from [a JUCE download](https://github.com/juce-framework/JUCE/releases/tag/8.0.12) in the same level as this folder; and then just run ./build.sh .

MacOS:
Download and add the JUCE folder as above, and install cmake:
'''
brew install cmake
'''
Remove the parts of the CMake file that call out Linux binaries specifically, and then simply run the build.sh script.

Windows:
Download and add the JUCE folder as above, putting it into the same folder as the repo. I decided to use VS Code, so install that, and then download and install the build tools for Visual Studio https://visualstudio.microsoft.com/downloads/ .

Open your six_sounds folder in VS Code.
ctrl+shift+p, CMake: Select a Kit, and select the VSCode stuff you just installed.
Remove the parts of the CMake file that call out Linux binaries specifically.
... Click build.
