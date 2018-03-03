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
        struct NowSound_DeviceInfo
        {
            LPWSTR Id;
            LPWSTR Name;

            // Construct a DeviceInfo; it will directly reference the given dictionary (no copying).
            // Note that this does *not* own the strings; these must be owned elsewhere.
            NowSound_DeviceInfo(LPWSTR id, LPWSTR name)
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

        enum NowSoundGraphState
        {
            // InitializeAsync() has not yet been called.
            Uninitialized,

            // Some error has occurred; GetLastError() will have details.
            InError,

            // A gap in incoming audio data was detected; this should ideally never happen.
            // NOTYET: Discontinuity,

            // InitializeAsync() has completed; devices can now be queried.
            Initialized,

            // CreateAudioGraphAsync() has completed; other methods can now be called.
            Created,

            // The audio graph has been started and is running.
            Running,

            // The audio graph has been stopped.
            // NOTYET: Stopped,
        };

        // The state of a particular IHolofunkAudioTrack.
        enum NowSoundTrackState
        {
            // The track is being recorded and it is not known when it will finish.
            Recording,

            // The track is finishing off its now-known recording time.
            FinishRecording,

            // The track is playing back, looping.
            Looping,
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
