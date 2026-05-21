Important Note:
This compiles but also it will wreck everything in your audio chain, so please don't use it yet haha!

"Structure like this:"
SpectralResonator/
├── CMakeLists.txt
└── Source/
    ├── CMakeLists.txt
    ├── SpectralEngine.h
    ├── PluginProcessor.h
    └── PluginProcessor.cpp
"Then build like this"
rm -rf build
mkdir build && cd build
cmake ..
cmake --build . --config Release

I have not checked out or tested any of this plugin's code as of yet. but I wanted to move things around so I could just try things within this context, and maybe learn via reading some of the math around this.
