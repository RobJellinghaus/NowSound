# NowSound
Low latency audio library for Windows 10, targeted at Unity UWP and desktop apps.

# WARNING: THIS LIBRARY IS JUST BEGINNING ITS DEVELOPMENT AND DOES NOT ACTUALLY WORK YET.
# THIS WARNING WILL BE REMOVED AS SOON AS THIS README BECOMES ACTUALLY TRUE :-)
# (status as of 13 January 2018)

NowSound is a wrapper library around the Windows 10 [AudioGraph](https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/audio-graphs)
API.  It exposes a P/Invoke C-style API, such that it can be invoked by Unity apps (on either
the .NET or Mono runtimes).

NowSound provides the following APIs:

- Initializing audio subsystem
- Querying input/output devices
- Setting up audio graph with given input/output devices
- Starting and stopping recording of an input to a memory stream
- Playing back audio from a memory stream (with looping)

[VST](https://en.wikipedia.org/wiki/Virtual_Studio_Technology) support is planned, in the desktop
version of the library.  (UWP security restrictions are not friendly to most current VST plugins.) 

I implemented NowSound because I needed lower-latency audio than is available in Unity's audio
subsystems, and because I needed to ensure all audio processing was happening natively.  On tight
low-latency audio deadlines, driving audio from C# (or any garbage-collected language) is sure to
cause audible trouble at some point.

NowSound is implemented using the amazing [cppwinnrt](https://github.com/Microsoft/cppwinrt)
library, which makes Windows component-based programming more pleasant than I ever thought
C++ could be.  I used Kenny Kerr's great [examples](https://github.com/kennykerr/cppwinrt) as a
starting point.

If you are interested in NowSound, check out the project which motivated me to write it:
[my gestural mixed reality live looper, named Holofunk](http://holofunk.com).
