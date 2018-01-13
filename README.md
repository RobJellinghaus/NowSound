# NowSound
Low latency audio library for Windows 10, targeted at Unity UWP and desktop apps.

NowSound is a wrapper library around the Windows 10 [AudioGraph](https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/audio-graphs)
API.  It exposes a P/Invoke C-style API, such that it can be invoked by Unity apps (on either
the .NET or Mono runtimes).

NowSound provides the following APIs:

- Initializing audio subsystem
- Querying input/output devices
- Setting up audio graph with given input/output devices
- Starting and stopping recording of an input to a memory stream
- Playing back audio from a memory stream (with looping)

VST support is planned, in the desktop version of the library.  (UWP security restrictions
are not friendly to most current VST plugins.) 
