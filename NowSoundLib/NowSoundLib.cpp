//
// NowSoundLib.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "NowSoundGraph.h"
#include "NowSoundInput.h"
#include "NowSoundLib.h"
#include "NowSoundTrack.h"

namespace NowSound
{
    NowSoundGraphInfo NowSoundGraph_GetStaticGraphInfo()
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

    NowSoundTimeInfo NowSoundGraph_GetStaticTimeInfo()
    {
        return CreateNowSoundTimeInfo(
            // JUCETODO: 1,
            1,
            (float)2,
            (float)3,
            (int)4,
            (float)5);
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

    void NowSoundGraph_InitializeInstance(
        int outputBinCount,
        float centralFrequency,
        int octaveDivisions,
        int centralBinIndex,
        int fftSize,
        float preRecordingDuration)
    {
        Check(NowSoundGraph_State() == NowSoundGraphState::GraphUninitialized);
        NowSoundGraph::InitializeInstance(
            outputBinCount,
            centralFrequency,
            octaveDivisions,
            centralBinIndex,
            fftSize,
            preRecordingDuration);
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
            NowSoundLogInfo info = NowSoundGraph::Instance()->LogInfo();
            return info;
        }
        else
        {
            return NowSoundLogInfo{};
        }
    }

    void NowSoundGraph_GetLogMessage(int32_t logMessageIndex, LPWSTR buffer, int32_t bufferCapacity)
    {
        // externally, this can only be called once Initialize is complete
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->GetLogMessage(logMessageIndex, buffer, bufferCapacity);
    }

    void NowSoundGraph_DropLogMessages(int32_t messageCountToDrop)
    {
        // externally, this can only be called once Initialize is complete
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->DropLogMessages(messageCountToDrop);
    }

    void NowSoundGraph_LogConnections()
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->LogConnections();
    }

    NowSoundSignalInfo NowSoundGraph_RawInputSignalInfo(AudioInputId audioInputId)
    {
        if (NowSoundGraph::Instance() != nullptr)
        {
            return NowSoundGraph::Instance()->Input(audioInputId)->SignalInfo();
        }
        else
        {
            return NowSoundSignalInfo{};
        }
    }

    NowSoundSignalInfo NowSoundGraph_InputSignalInfo(AudioInputId audioInputId)
    {
        if (NowSoundGraph::Instance() != nullptr)
        {
            return NowSoundGraph::Instance()->Input(audioInputId)->SignalInfo();
        }
        else
        {
            return NowSoundSignalInfo{};
        }
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

    float NowSoundGraph_InputPan(AudioInputId audioInputId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->InputPan(audioInputId);
    }

    void NowSoundGraph_SetInputPan(AudioInputId audioInputId, float pan)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->InputPan(audioInputId, pan);
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

    void NowSoundGraph_SetTempo(float beatsPerMinute, int beatsPerMeasure)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->SetTempo(beatsPerMinute, beatsPerMeasure);
    }

    void NowSoundGraph_GetInputFrequencies(AudioInputId audioInputId, void* floatBuffer, int32_t floatBufferCapacity)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->Input(audioInputId)->GetFrequencies(floatBuffer, floatBufferCapacity);
    }

    NowSoundSpatialParameters NowSoundGraph_SpatialParameters(AudioInputId audioInputId)
    {
        return NowSoundGraph::Instance()->Input(audioInputId)->SpatialParameters();
    }

    TrackId NowSoundGraph_CreateRecordingTrackAsync(AudioInputId audioInputId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->CreateRecordingTrackAsync(audioInputId);
    }

    TrackId NowSoundGraph_CopyLoopingTrack(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->CopyLoopingTrack(trackId);
    }

    void NowSoundGraph_DeleteTrack(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->DeleteTrack(trackId);
    }

    void NowSoundGraph_MessageTick()
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->MessageTick();
    }

    void NowSoundGraph_StartRecording(LPWSTR fileName, int32_t fileNameLength)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->StartRecording(fileName, fileNameLength);
    }

    void NowSoundGraph_StopRecording()
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->StopRecording();
    }

    // Plugin searching requires setting paths to search.
    // TODO: make this use the idiom for passing in strings rather than StringBuilders.
    void NowSoundGraph_AddPluginSearchPath(LPWSTR wcharBuffer, int32_t bufferCapacity)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->AddPluginSearchPath(wcharBuffer, bufferCapacity);
    }

    bool NowSoundGraph_SearchPluginsSynchronously()
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->SearchPluginsSynchronously();
    }

    int NowSoundGraph_PluginCount()
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->PluginCount();
    }

    void NowSoundGraph_PluginName(PluginId pluginId, LPWSTR wcharBuffer, int32_t bufferCapacity)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->PluginName(pluginId, wcharBuffer, bufferCapacity);
    }

    bool NowSoundGraph_LoadPluginPrograms(PluginId pluginId, LPWSTR pathnameBuffer)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->LoadPluginPrograms(pluginId, pathnameBuffer);
    }

    int NowSoundGraph_PluginProgramCount(PluginId pluginId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->PluginProgramCount(pluginId);
    }

    // Get the name of the specified plugin's program.  Note that IDs are 1-based.
    void NowSoundGraph_PluginProgramName(PluginId pluginId, ProgramId programId, LPWSTR wcharBuffer, int32_t bufferCapacity)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->PluginProgramName(pluginId, programId, wcharBuffer, bufferCapacity);
    }

    // Add an instance of the given plugin on the given input.
    PluginInstanceIndex NowSoundGraph_AddInputPluginInstance(AudioInputId audioInputId, PluginId pluginId, ProgramId programId, int32_t dryWet_0_100)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Input(audioInputId)->AddPluginInstance(pluginId, programId, dryWet_0_100);
    }

    int NowSoundGraph_GetInputPluginInstanceCount(AudioInputId audioInputId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Input(audioInputId)->GetPluginInstanceCount();
    }

    NowSoundPluginInstanceInfo NowSoundGraph_GetInputPluginInstanceInfo(AudioInputId audioInputId, PluginInstanceIndex index)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Input(audioInputId)->GetPluginInstanceInfo(index);
    }

    void NowSoundGraph_SetInputPluginInstanceDryWet(AudioInputId audioInputId, PluginInstanceIndex pluginInstanceIndex, int32_t dryWet_0_100)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->Input(audioInputId)->SetPluginInstanceDryWet(pluginInstanceIndex, dryWet_0_100);
    }

    void NowSoundGraph_DeleteInputPluginInstance(AudioInputId audioInputId, PluginInstanceIndex pluginInstanceIndex)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->Input(audioInputId)->DeletePluginInstance(pluginInstanceIndex);
    }
    
    void NowSoundGraph_ShutdownInstance()
    {
        // In this one case we decide to tolerate shutting down before ever starting up.
        if (NowSoundGraph::Instance() != nullptr)
        {
            NowSoundGraph::ShutdownInstance();
        }
    }

    NowSoundTrackInfo NowSoundTrack_GetStaticTrackInfo()
    {
        return CreateNowSoundTrackInfo(
            // don't pass 0 for zeroth (bool) field
            true,
            2,
            (float)3,
            (float)4,
            (float)5,
            (float)6,
            (float)7,
            (float)8,
            9);
    }

    NowSoundTrackState NowSoundTrack_State(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->State();
    }

    int64_t /*Duration<Beat>*/ NowSoundTrack_BeatDuration(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->BeatDuration().Value();
    }

    float /*ContinuousDuration<Beat>*/ NowSoundTrack_BeatPositionUnityNow(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->BeatPositionUnityNow().Value();
    }

    float /*ContinuousDuration<AudioSample>*/ NowSoundTrack_ExactDuration(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->ExactDuration().Value();
    }

    NowSoundTrackInfo NowSoundTrack_Info(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->Info();
    }

    NowSoundSignalInfo NowSoundTrack_SignalInfo(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->SignalInfo();
    }

    void NowSoundTrack_FinishRecording(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->Track(trackId)->FinishRecording();
    }

    void NowSoundTrack_GetFrequencies(TrackId trackId, void* floatBuffer, int32_t floatBufferCapacity)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        if (NowSoundGraph::Instance()->TrackIsDefined(trackId))
        {
            NowSoundGraph::Instance()->Track(trackId)->GetFrequencies(floatBuffer, floatBufferCapacity);
        }
        else
        {
            NowSoundGraph::Instance()->Log(L"Track ID *WAS NOT DEFINED* in NowSoundTrack_GetFrequencies");
        }
    }

    bool NowSoundTrack_IsMuted(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->IsMuted();
    }

    void NowSoundTrack_SetIsMuted(TrackId trackId, bool isMuted)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->Track(trackId)->IsMuted(isMuted);
    }

    float NowSoundTrack_Pan(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->Pan();
    }

    void NowSoundTrack_SetPan(TrackId trackId, float pan)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->Track(trackId)->Pan(pan);
    }

    float NowSoundTrack_Volume(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->Volume();
    }

    void NowSoundTrack_SetVolume(TrackId trackId, float volume)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->Track(trackId)->Volume(volume);
    }

    PluginInstanceIndex NowSoundTrack_AddPluginInstance(TrackId trackId, PluginId pluginId, ProgramId programId, int32_t dryWet_0_100)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->AddPluginInstance(pluginId, programId, dryWet_0_100);
    }

    int NowSoundTrack_GetPluginInstanceCount(TrackId trackId)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->GetPluginInstanceCount();
    }

    NowSoundPluginInstanceInfo NowSoundTrack_GetPluginInstanceInfo(TrackId trackId, PluginInstanceIndex index)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->GetPluginInstanceInfo(index);
    }

    void NowSoundTrack_SetPluginInstanceDryWet(TrackId trackId, PluginInstanceIndex PluginInstanceIndex, int32_t dryWet_0_100)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        NowSoundGraph::Instance()->Track(trackId)->SetPluginInstanceDryWet(PluginInstanceIndex, dryWet_0_100);
    }

    void NowSoundTrack_DeletePluginInstance(TrackId trackId, PluginInstanceIndex PluginInstanceIndex)
    {
        Check(NowSoundGraph::Instance() != nullptr);
        return NowSoundGraph::Instance()->Track(trackId)->DeletePluginInstance(PluginInstanceIndex);
    }
}
