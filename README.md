# NowSound
Low latency audio library for Windows 10, targeted at Unity UWP and desktop apps.

# WARNING: THIS LIBRARY IS JUST BEGINNING ITS DEVELOPMENT AND DOES NOT ACTUALLY WORK YET. THIS WARNING WILL BE REMOVED AS SOON AS THIS README BECOMES FACTUALLY CORRECT :-)

NowSound is a wrapper library around the Windows 10 [AudioGraph](https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/audio-graphs)
API.  It exposes a P/Invoke C-style API, such that it can be invoked by Unity apps (on either
the .NET or Mono runtimes).

NowSound provides the following APIs:

- Initializing audio subsystem
- Querying input/output devices
- Setting up audio graph with given input/output devices
- Starting and stopping recording of an input to an in-memory audio track
- Playing back in-memory audio tracks (with optional looping)

All of these APIs can be asynchronous.  Currently NowSound makes no attempt to implement event or
callback support; the only way to track the state of NowSound is by polling.  This actually works
reasonably well for Unity applications which are based on a real-time event loop anyway.  Callback
support will be added if particular applications require it.

NowSound consists of the following subprojects:

- NowSoundLib: the C++ NowSound library itself
- NowSoundLibDotNet: a .NET C# wrapper library using P/Invoke to load and call NowSoundLib
- NowSoundAppUWP: a C++ UWP demonstration app using NowSoundLib
- NowSoundTestClr: a C# TAEF testing library for NowSoundLibDotNet
- NowSoundUnityUWP: a Unity demonstration app targeting Windows UWP
- NowSoundUnityDesktop: a Unity demonstration app targeting Windows desktop

Yes, the demonstration and test apps just about outnumber the library itself, but the whole point
here is to verify this actually works in all these combinations.

Note that any pull requests must ensure that all tests are passing.

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

*Note as of January 2018:* The cppwinrt libraries are present only in Windows Insider preview
builds of Windows, so if you are not ready to install such a build, you will not be able to build
this code.  This code currently targets the latest Windows 10 SDK version, namely [17061](https://blogs.windows.com/buildingapps/2017/12/19/windows-10-sdk-preview-build-17061-now-available/#ml17ACbvB2HZPo5J.97).

The next major Windows 10 release in spring 2018 will come with an SDK that will
support building this project; at that time, I'll remove this warning (and replace it with a
warning stating that the latest Win10 version is required for building).

If you are interested in NowSound, check out the project which motivated me to write it:
[my gestural Kinect-and-mixed-reality live looper, Holofunk](http://holofunk.com).  You might also
enjoy [my blog](http://robjsoftware.info) and in particular [this post about why I need
this sound library for my Holofunk project](https://robjsoftware.info/2017/12/02/big-ol-2017-mega-update-progress-almost-entirely-invisible/).
(Though it turns out that I was wrong about one thing there: AudioGraph is *not* managed
only, and when I realized this, I realized AudioGraph was the path of least resistance
for this library.)
