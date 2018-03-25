# NowSound
Low latency audio library for Windows 10, targeted at Unity UWP and desktop apps.

NowSound is a wrapper library around the Windows 10 [AudioGraph](https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/audio-graphs)
API.  It exposes a P/Invoke C-style API, such that it can be invoked by Unity apps (on either
the .NET or Mono runtimes).

**Status as of late March 2018: the library basically works and has a UWP demonstration
mini-looper app, but some features remain to be completed such as device selection,
track muting/deleting in the demo app, and most importantly VST support.  Pull requests
welcome!**

## APIs

NowSound provides the following APIs:

- Initializing audio subsystem
- Setting up audio graph with default input/output devices
- Starting and stopping recording of an input to an in-memory audio track (quantized to a regular tempo)
- Looping playback of in-memory audio tracks

All of these APIs are asynchronous.  Currently NowSound makes no attempt to implement event or
callback support; the only way to track the state of NowSound is by polling.  This actually works
reasonably well for Unity applications which are based on a real-time event loop anyway.  Callback
support will be added if particular applications require it.

## Project structure

NowSound consists of the following subprojects:

- NowSoundLibShared: the core C++ classes for streaming and buffering
- NowSoundLib: a UWP C++ library sharing NowSoundLibShared and invoking AudioGraph,
  exposing a P/Invoke interface
- NowSoundAppUWP: a C++ UWP demonstration app using NowSoundLib, showing how to start
  recording and looping multiple tracks
- UnitTestsDesktop: a C++ TAEF testing library for the NowSoundLibShared code

Note that any pull requests must ensure that all tests are passing.

[VST](https://en.wikipedia.org/wiki/Virtual_Studio_Technology) support is planned, in the desktop
version of the library.  (UWP security restrictions are not friendly to most current VST plugins.) 

## Dependencies and Building

NowSound is implemented using the amazing [cppwinnrt](https://github.com/Microsoft/cppwinrt)
library, which makes Windows component-based programming more pleasant than I ever thought
C++ could be.  I used Kenny Kerr's great [examples](https://github.com/kennykerr/cppwinrt) as a
starting point.  The most impressive thing about C++/WinRT is how incredibly easy it makes
multi-threaded concurrent programming -- using the co_await expression in C++ is literally as
easy as C#.

*Note as of March 2018:* The cppwinrt libraries are present only in Windows Insider preview
builds of Windows, so if you are not ready to install such a build, you will not be able to build
this code.  This code currently targets the latest Windows 10 SDK version, namely [17061](https://blogs.windows.com/buildingapps/2017/12/19/windows-10-sdk-preview-build-17061-now-available/#ml17ACbvB2HZPo5J.97).

The next major Windows 10 release in spring 2018 will come with an SDK that will
support building this project; at that time, I'll remove this warning (and replace it with a
warning stating that the latest Win10 version is required for building).

## Rationale

I implemented NowSound because I needed lower-latency audio than is available in Unity's audio
subsystems, and because I needed to ensure all audio processing was happening natively.  On tight
low-latency audio deadlines, driving audio from C# (or any garbage-collected language) is sure to
cause audible trouble at some point.

The current performance is far better than I was ever able to get while driving AudioGraph from
a C# app -- the current app runs the audio graph at the minimum available latency, which in the
case of my Surface Book set to 48Khz sampling rate, gives a 96-sample buffer (200Hz audio frame
rate) with no perceptible audio problems streaming multiple loops.

If you are interested in NowSound, check out the project which motivated me to write it:
[my gestural Kinect-and-mixed-reality live looper, Holofunk](http://holofunk.com).  You might also
enjoy [my blog](http://robjsoftware.info) and in particular [this post about why I need
this sound library for my Holofunk project](https://robjsoftware.info/2017/12/02/big-ol-2017-mega-update-progress-almost-entirely-invisible/).
(Though it turns out that I was wrong about one thing there: AudioGraph is *not* managed
only, and when I realized this, I realized AudioGraph was the path of least resistance
for this library.)

## Code Patterns

It's worth mentioning some aspects of the code that are pretty important to developing and
maintaining it:

### Modern C++ Ownership

There is not a single explicit C++ `delete` statement in this repository.  Using std::unique_ptr,
std::move, and rvalue `&&` references means that the code is able to handle all buffer management
efficiently and correctly, without reference counting or GC overhead and without explicit deletion.

### Generic Types for Units

In my early audio programming experience with C, I learned a deep fear of the "int" data type.
Integers (either short, normal, or long) were used for sample counts, byte counts, numbers of
ticks, timestamps, milliseconds, and every other quantity.  Getting confused was easy to do
and hard to debug.

This code uses a common pattern, defining generic types `Time<T>` and `Duration<T>`.  The "T"
generic parameter serves as a placeholder for the type of time/duration involved.  For instance,
`Time<Sample>` defines a timestamp in terms of a count of audio samples since the start of the
program.  `Duration<Sample>` defines a time interval in terms of a number of audio samples.
Arithmetic operators exist to add Time + Duration, subtract Duration from Time, subtract Time
from Time giving a Duration, etc., provided that the T parameters match; you can't subtract
a `Time<Sample>` from a `Time<Seconds>` as the compiler won't let you.

This may look verbose, but it precludes so many kinds of bugs that it has been well worth
doing.