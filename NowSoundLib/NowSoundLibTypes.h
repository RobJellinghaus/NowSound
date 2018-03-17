// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

#include <future>

#include "stdint.h"

#include "Check.h"

using namespace std::chrono;
using namespace winrt;

using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;
using namespace Windows::System;

namespace NowSound
{
    // All external methods here are static and use C linkage, for P/Invokability.
    // AudioGraph object references are passed by integer ID; lifecycle is documented
    // per-API.
    // Callbacks are implemented by passing statically invokable callback hooks
    // together with callback IDs.
    extern "C"
    {
        // Information about an audio device.
        struct NowSoundDeviceInfo
        {
            LPWSTR Id;
            LPWSTR Name;

            // Construct a DeviceInfo; it will directly reference the given dictionary (no copying).
            // Note that this does *not* own the strings; these must be owned elsewhere.
            NowSoundDeviceInfo(LPWSTR id, LPWSTR name)
            {
                Id = id;
                Name = name;
            }
        };

        // Information about an audio graph.
        struct NowSoundGraphInfo
        {
            int32_t LatencyInSamples;
            int32_t SamplesPerQuantum;

            NowSoundGraphInfo(int32_t latencyInSamples, int32_t samplesPerQuantum)
            {
                LatencyInSamples = latencyInSamples;
                SamplesPerQuantum = samplesPerQuantum;
            }
        };

        // The states of a NowSound graph.
        // Note that since this is extern "C", this is not an enum class, so these identifiers have to begin with Track
        // to disambiguate them from the TrackState identifiers.
        enum NowSoundGraphState
        {
            // InitializeAsync() has not yet been called.
            GraphUninitialized,

            // Some error has occurred; GetLastError() will have details.
            GraphInError,

            // A gap in incoming audio data was detected; this should ideally never happen.
            // NOTYET: Discontinuity,

            // InitializeAsync() has completed; devices can now be queried.
            GraphInitialized,

            // CreateAudioGraphAsync() has completed; other methods can now be called.
            GraphCreated,

            // The audio graph has been started and is running.
            GraphRunning,

            // The audio graph has been stopped.
            // NOTYET: Stopped,
        };

        // The state of a particular IHolofunkAudioTrack.
        // Note that since this is extern "C", this is not an enum class, so these identifiers have to begin with Track
        // to disambiguate them from the GraphState identifiers.
        enum NowSoundTrackState
        {
            // This track is not initialized -- important for some state machine cases and for catching bugs
            // (also important that this be the default value)
            TrackUninitialized,

            // The track is being recorded and it is not known when it will finish.
            TrackRecording,

            // The track is finishing off its now-known recording time.
            TrackFinishRecording,

            // The track is playing back, looping.
            TrackLooping,
        };

        // The audio inputs known to the app.
        /// Prevents confusing an audio input with some other int value.
        // 
        // The predefined values are really irrelevant; it can be cast to and from int as necessary.
        // But, used in parameters, the type helps with making the code self-documenting.
        enum AudioInputId
        {
            Input0,
            Input1,
            Input2,
            Input3,
            Input4,
            Input5,
            Input6,
            Input7,
        };

        // The ID of a NowSound track; avoids issues with marshaling object references.
        // Note that 0 is a valid value.
        typedef int32_t TrackId;
    };
}
