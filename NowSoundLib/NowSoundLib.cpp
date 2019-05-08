//
// NowSoundLib.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "NowSoundLib.h"
#include "NowSoundGraph.h"
#include "NowSoundTrack.h"

namespace NowSound
{
	__declspec(dllexport) NowSoundGraphInfo NowSoundGraph_GetStaticGraphInfo()
	{
		return CreateNowSoundGraphInfo(
			1,
			2,
			3,
			4,
			5
			// JUCETODO: , 6
		);
	}

	__declspec(dllexport) NowSoundTimeInfo NowSoundGraph_GetStaticTimeInfo()
	{
		return CreateNowSoundTimeInfo(
			// JUCETODO: 1,
			1,
			(float)2,
			(float)3,
			(float)4);
	}

	NowSoundGraphState NowSoundGraph_State()
	{
		if (NowSoundGraph::Instance() == nullptr)
		{
			return NowSoundGraphState::GraphUninitialized;
		}
		else
		{
			return NowSoundGraph::Instance()->State();
		}
	}

	void NowSoundGraph_InitializeInstance()
	{
		Check(NowSoundGraph_State() == NowSoundGraphState::GraphUninitialized);
		NowSoundGraph::InitializeInstance();
	}

	NowSoundGraphInfo NowSoundGraph_Info()
	{
		// externally, this can only be called once Initialize is complete; internally, NowSoundGraph::Info() is called *during* Initialize
		if (NowSoundGraph::Instance() != nullptr)
		{
			return NowSoundGraph::Instance()->Info();
		}
		else
		{
			return NowSoundGraphInfo{};
		}
	}

	NowSoundLogInfo NowSoundGraph_LogInfo()
	{
		// externally, this can only be called once Initialize is complete
		if (NowSoundGraph::Instance() != nullptr)
		{
			return NowSoundGraph::Instance()->LogInfo();
		}
		else
		{
			return NowSoundLogInfo{};
		}
	}

	void NowSoundGraph_GetLogMessage(int32_t logMessageIndex, LPWSTR buffer, int32_t bufferCapacity)
	{
		// externally, this can only be called once Initialize is complete
		Check(NowSoundGraph::Instance()->State() > NowSoundGraphState::GraphUninitialized);
		NowSoundGraph::Instance()->GetLogMessage(logMessageIndex, buffer, bufferCapacity);
	}

	void NowSoundGraph_DropLogMessagesUpTo(int32_t logMessageIndex)
	{
		// externally, this can only be called once Initialize is complete
		Check(NowSoundGraph::Instance()->State() > NowSoundGraphState::GraphUninitialized);
		NowSoundGraph::Instance()->DropLogMessagesUpTo(logMessageIndex);
	}

	NowSoundSignalInfo NowSoundGraph_OutputSignalInfo()
    {
        if (NowSoundGraph::Instance() != nullptr)
		{
			return NowSoundGraph::Instance()->OutputSignalInfo();
		}
		else
		{
			return NowSoundSignalInfo{};
		}
    }

#ifdef INPUT_DEVICE_SELECTION // JUCETODO
	void NowSoundGraph_InputDeviceId(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity)
	{
		NowSoundGraph::Instance()->InputDeviceId(deviceIndex, wcharBuffer, bufferCapacity);
	}

	void NowSoundGraph_InputDeviceName(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity)
	{
		NowSoundGraph::Instance()->InputDeviceName(deviceIndex, wcharBuffer, bufferCapacity);
	}

	void NowSoundGraph_InitializeDeviceInputs(int deviceIndex)
	{
		NowSoundGraph::Instance()->InitializeDeviceInputs(deviceIndex);
	}
#endif

	void NowSoundGraph_InitializeFFT(
		int outputBinCount,
		float centralFrequency,
		int octaveDivisions,
		int centralBinIndex,
		int fftSize)
	{
		NowSoundGraph::Instance()->InitializeFFT(outputBinCount, centralFrequency, octaveDivisions, centralBinIndex, fftSize);
	}

	NowSoundTimeInfo NowSoundGraph_TimeInfo()
	{
		if (NowSoundGraph::Instance() != nullptr)
		{
			return NowSoundGraph::Instance()->TimeInfo();
		}
		else
		{
			return NowSoundTimeInfo{};
		}
	}

#ifdef INPUT_DEVICE_SELECTION
	NowSoundInputInfo NowSoundGraph_InputInfo(AudioInputId audioInputId)
	{
		return NowSoundGraph::Instance()->InputInfo(audioInputId);
	}
#endif

	void NowSoundGraph_PlayUserSelectedSoundFileAsync()
	{
		Check(NowSoundGraph::Instance() != nullptr);
		NowSoundGraph::Instance()->PlayUserSelectedSoundFileAsync();
	}

	TrackId NowSoundGraph_CreateRecordingTrackAsync(AudioInputId audioInputId)
	{
		Check(NowSoundGraph::Instance() != nullptr);
		return NowSoundGraph::Instance()->CreateRecordingTrackAsync(audioInputId);
	}

	void NowSoundGraph_ShutdownInstance()
	{
		Check(NowSoundGraph::Instance() != nullptr);
		NowSoundGraph::ShutdownInstance();
	}

	__declspec(dllexport) NowSoundTrackInfo NowSoundTrack_GetStaticTrackInfo()
	{
		return CreateNowSoundTrackInfo(
			// don't pass 0 for zeroth (bool) field
            true,
			2,
			(float)3,
			4,
			5,
			(float)6,
			7,
			(float)8,
			9,
			(float)10);
	}

	__declspec(dllexport) NowSoundTrackState NowSoundTrack_State(TrackId trackId)
	{
		Check(NowSoundGraph::Instance() != nullptr);
		return NowSoundTrackAudioProcessor::Track(trackId)->State();
	}

	__declspec(dllexport) int64_t /*Duration<Beat>*/ NowSoundTrack_BeatDuration(TrackId trackId)
	{
		Check(NowSoundGraph::Instance() != nullptr);
		return NowSoundTrackAudioProcessor::Track(trackId)->BeatDuration().Value();
	}

	__declspec(dllexport) float /*ContinuousDuration<Beat>*/ NowSoundTrack_BeatPositionUnityNow(TrackId trackId)
	{
		Check(NowSoundGraph::Instance() != nullptr);
		return NowSoundTrackAudioProcessor::Track(trackId)->BeatPositionUnityNow().Value();
	}

	__declspec(dllexport) float /*ContinuousDuration<AudioSample>*/ NowSoundTrack_ExactDuration(TrackId trackId)
	{
		Check(NowSoundGraph::Instance() != nullptr);
		return NowSoundTrackAudioProcessor::Track(trackId)->ExactDuration().Value();
	}

	__declspec(dllexport) NowSoundTrackInfo NowSoundTrack_Info(TrackId trackId)
	{
		Check(NowSoundGraph::Instance() != nullptr);
		return NowSoundTrackAudioProcessor::Track(trackId)->Info();
	}

	__declspec(dllexport) NowSoundSignalInfo NowSoundTrack_SignalInfo(TrackId trackId)
	{
		Check(NowSoundGraph::Instance() != nullptr);
		return NowSoundTrackAudioProcessor::Track(trackId)->SignalInfo();
	}

	__declspec(dllexport) void NowSoundTrack_FinishRecording(TrackId trackId)
	{
		Check(NowSoundGraph::Instance() != nullptr);
		NowSoundTrackAudioProcessor::Track(trackId)->FinishRecording();
	}

	__declspec(dllexport) void NowSoundTrack_GetFrequencies(TrackId trackId, void* floatBuffer, int floatBufferCapacity)
	{
		Check(NowSoundGraph::Instance() != nullptr);
		NowSoundTrackAudioProcessor::Track(trackId)->GetFrequencies(floatBuffer, floatBufferCapacity);
	}

	__declspec(dllexport) bool NowSoundTrack_IsMuted(TrackId trackId)
	{
		Check(NowSoundGraph::Instance() != nullptr);
		return NowSoundTrackAudioProcessor::Track(trackId)->IsMuted();
	}

	__declspec(dllexport) void NowSoundTrack_SetIsMuted(TrackId trackId, bool isMuted)
	{
		Check(NowSoundGraph::Instance() != nullptr);
		NowSoundTrackAudioProcessor::Track(trackId)->IsMuted(isMuted);
	}

	__declspec(dllexport) void NowSoundTrack_Delete(TrackId trackId)
	{
		Check(NowSoundGraph::Instance() != nullptr);
		NowSoundTrackAudioProcessor::DeleteTrack(trackId);
	}
}
