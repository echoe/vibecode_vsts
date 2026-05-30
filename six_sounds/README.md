A synthesizer with six sounds. You mix them together. Then you get a lotta sounds.

Synth features:

-Different oscillator types!
Wave: Sine, Triangle, Saw, Pulse, White Noise
You can change the Ratio, detune the wave, change the phase of the wave, and change the gain of each oscillator (that's how you control whether or not the oscillator makes sound, which is important if you route a bunch of oscillators into other oscillators!).

Additive: Ratio, Tilt, Stretch, Odd/Even
It's an actual additive oscillator in your FM oscillator! Additive is cool. I think this is /very cool/.

Filter types: Lowpass, Highpass, Comb, Granular.
--You can change the cutoff, resonance, keytracking, and feedback of each filter. Granular has its own unique settings:
---Granular: Grain size, Damping, Scatter, Feedback

-You can sync an operator to DAW tempo and use it as an LFO. Do you want more LFOs? Why have LFOs when you can just have more oscillators??

-You can pass audio into an operator with the audio routing matrix, so you can use an operator as a filter. Do you want more filters? Why have more filters when you can just have more operators?

-You can modulate any operator with any other operator. Eventually there will also be a little mod matrix but it's in process.

-Three simple, global-only effects to soften up the FM; Delay, Reverb, and Chorus.

-Preset support, in the form of writing xml files, so how much support is it really?

-Randomization of each individual page.

-An Init button to avoid the randomization and get back to a normal state when it becomes too much.

-A gain function to attempt to control the noisiness of FM (optimistic).

-A soft clipper on the end of the chain and at every step of the audio routing page.

Pictures of the synth in use are provided in the pictures/ folder, if you are curious. Builds are provided in the binaries/ folder.

This is mostly AI-coded, but I've also done quite a lot on my end.

It builds on my machine on Windows, MacOS, and Linux.
Binaries available here, but they are a bit broken: https://github.com/echoe/vibecode_vsts/releases I will build again after I go through another song cycle. You can always build yourself - these builds don't have working effects and don't properly reset when looping in a DAW, among other things.

I've added patches but I made these patches while making a song with the synth and was rebuilding the synth to fix bugs as I was doing so, so I can't guarantee that the patches sound fine yet! That's a release feature.


Roadmap features:
-Working mod matrix. This will add PWM (modulate the phase. Isn't that what PWM actually is?). I may also need to add a tweak to make operators go even slower to make this work ... but I'll see how it goes.
