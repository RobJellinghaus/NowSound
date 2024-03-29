// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include <algorithm>

#include "Clock.h"
#include "GetBuffer.h"
#include "Histogram.h"
#include "MagicConstants.h"
#include "NowSoundLib.h"
#include "NowSoundGraph.h"
#include "NowSoundInput.h"
#include "NowSoundTrack.h"
#include "Option.h"
#include "Tempo.h"

using namespace concurrency;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace winrt::Windows::Foundation;

bool juce::NowSound_CheckLogThrottle()
{
    return NowSound::NowSoundGraph::Instance()->CheckLogThrottle();
}

void juce::NowSound_Log(const std::wstring& message)
{
    NowSound::NowSoundGraph::Instance()->Log(message);
}

namespace NowSound
{
    TimeSpan timeSpanFromSeconds(int seconds)
    {
        // TimeSpan is in 100ns units
        return TimeSpan(seconds * Clock::TicksPerSecond);
    }

    std::unique_ptr<NowSoundGraph> NowSoundGraph::s_instance{ nullptr };

    NowSoundGraph* NowSoundGraph::Instance() { return s_instance.get(); }

    void NowSoundGraph::InitializeInstance(
        int outputBinCount,
        float centralFrequency,
        int octaveDivisions,
        int centralBinIndex,
        int fftSize,
        float preRecordingDuration)
    {
        std::unique_ptr<NowSoundGraph> temp{ new NowSoundGraph() };
        s_instance = std::move(temp);
        s_instance.get()->Initialize(
            outputBinCount,
            centralFrequency,
            octaveDivisions,
            centralBinIndex,
            fftSize,
            preRecordingDuration);
    }

    NowSoundGraph::NowSoundGraph() :
        _audioGraphState{ NowSoundGraphState::GraphUninitialized },
        _audioDeviceManager{},
        _audioAllocator{ nullptr },
        _clock{ nullptr },
        _nextTrackId{ TrackId::TrackIdUndefined },
        _nextAudioInputId{ AudioInputId::AudioInputUndefined },
        // JUCETODO: _inputDeviceIndicesToInitialize{},
        _audioInputs{ },
        _changingState{ false },
        _fftBinBounds{},
        _fftSize{ -1 },
        _stateMutex{},
        _outputSignalMutex{},
        _logMessages{},
        _logMutex{},
        _juceGraphChanged{},
        _juceGraphChangedMutex{},
        _audioPluginSearchPaths{},
        _knownPluginList{},
        _audioPluginFormatManager{},
        _preRecordingDuration{ 0 },
        _audioProcessorGraph{ new AudioProcessorGraph() },
        _tempo{ nullptr }
    {
        _logMessages.reserve(s_logMessageCapacity);
        Check(_logMessages.size() == 0);
    }

    NowSoundTrackAudioProcessor* NowSoundGraph::Track(TrackId id)
    {
        // NOTE THAT THIS PATTERN DOES NOT LOCK THE _tracks COLLECTION IN ANY WAY.
        // The only way this will be correct is if all modifications to s_tracks happen only as a result of
        // non-concurrent, serialized external calls to NowSoundTrackAPI.
        Check(id > TrackId::TrackIdUndefined);
        
        auto track = _tracks.find(id);
        Check(track != _tracks.end());
        NowSoundTrackAudioProcessor* value = track->second;
        Check(value != nullptr); // TODO: don't fail on invalid client values; instead return standard error code or something
        return value;
    }

    bool NowSoundGraph::TrackIsDefined(TrackId id)
    {
        // Race conditions can lead to a track being checked before it actually exists.
        // TODO: THIS SHOULD NOT BE THE CASE AND SHOULD BE FIXED.
        // For now, nonetheless, let's try this workaround and verify if it happens.
        return _tracks.find(id) != _tracks.end();
    }

    bool NowSoundGraph::CheckLogThrottle()
    {
        // TODO: revive this code if we need it
        /*
        int counter = _logThrottlingCounter;
        _logThrottlingCounter = ++_logThrottlingCounter % BaseAudioProcessor::LogThrottle;
        return counter == 0;
        */
        return false; // never, never print anything (for now)
    }

    // AudioGraph NowSoundGraph::GetAudioGraph() const { return _audioGraph; }

    // AudioDeviceOutputNode NowSoundGraph::AudioDeviceOutputNode() const { return _deviceOutputNode; }

    BufferAllocator<float>* NowSoundGraph::AudioAllocator() const { return _audioAllocator.get(); }

    void NowSoundGraph::PrepareToChangeState(NowSoundGraphState expectedState)
    {
        std::lock_guard<std::mutex> guard(_stateMutex);

        Check(_audioGraphState == expectedState);
        Check(!_changingState);
        _changingState = true;
    }

    void NowSoundGraph::ChangeState(NowSoundGraphState newState)
    {
        std::lock_guard<std::mutex> guard(_stateMutex);

        Check(_changingState);
        Check(newState != _audioGraphState);
        _changingState = false;
        _audioGraphState = newState;
    }

    NowSoundGraphState NowSoundGraph::State() const
    {
        // this is a machine word, atomically written; no need to lock
        return _audioGraphState;
    }

    NowSoundLogInfo NowSoundGraph::LogInfo()
    {
        // We don't actually need to synchronize with _logMutex in this method.
        // The only variable touched during log appending is the size of _logMessages, and it is inherently atomically updated. (WE THINK)
        NowSoundLogInfo info{};
        info.LogMessageCount = static_cast<int32_t>(_logMessages.size());
        return info;
    }

    // Add a log message.
    void NowSoundGraph::Log(const std::wstring& str)
    {
        std::lock_guard<std::mutex> guard(_logMutex);

        // This check should just never fail.  If it does, you're logging wrong.
        // The reason this check should never fail is that, if this can never cause a resize, then
        // it's safe for GetLogMessage to not lock the _logMessages vector itself -- which will hugely
        // reduce inter-thread contention.
        Check(_logMessages.size() < s_logMessageCapacity);

        _logMessages.push_back(str);
    }

    void NowSoundGraph::GetLogMessage(int32_t logMessageIndex, LPWSTR buffer, int32_t bufferCapacity)
    {
        Check(logMessageIndex < _logMessages.size());

        // We don't need to synchronize when getting the log message, so long as we never call DropLogMessages()
        // concurrently with this.
        const std::wstring& message = _logMessages.at(logMessageIndex);
        wcsncpy_s(buffer, (size_t)bufferCapacity, message.c_str(), message.size());
    }

    void NowSoundGraph::DropLogMessages(int32_t messageCountToDrop)
    {
        // This check does not need to be under a lock, under the assumptions that:
        // - this method is never called concurrently
        // - the value of size() is backed by an atomically updated int field
        // Under these assumptions, racing calls to Log (which are allowed) can only
        // make the count grow.  So if the count to drop is small enough outside the lock,
        // it will still be small enough inside the lock.
        Check(messageCountToDrop <= _logMessages.size());

        std::lock_guard<std::mutex> guard(_logMutex);

        _logMessages.erase(_logMessages.begin(), _logMessages.begin() + messageCountToDrop);
    }

    AudioProcessorGraph& NowSoundGraph::JuceGraph()
    {
        return *(_audioProcessorGraph.get());
    }

    void NowSoundGraph::setBufferSize()
    {
        // Set buffer size to appropriate value available on current device
        auto* device = _audioDeviceManager.getCurrentAudioDevice();

        auto bufferRate = device->getCurrentSampleRate();
        auto bufferSize = device->getCurrentBufferSizeSamples();

        const bool setMinBufferSize = true;
        if (setMinBufferSize)
        {
            auto bufferSizes = device->getAvailableBufferSizes();
            // OK, not smallest... next smallest... let's see if it helps static problems with >3 loops
            // ... it seems to! On Focusrite Scarlett 2i2, sizes are [16, 32, 48, 64, ...]
            int targetBufferSize = bufferSizes[3];

            AudioDeviceManager::AudioDeviceSetup setup;
            _audioDeviceManager.getAudioDeviceSetup(setup);

            if (setup.bufferSize != targetBufferSize)
            {
                setup.bufferSize = targetBufferSize;
                String result = _audioDeviceManager.setAudioDeviceSetup(setup, false);
                if (result.length() > 0)
                {
                    _exceptionMessage = std::string{"Audio device setup failed: "};
                    for (int i = 0; i < result.length(); i++)
                    {
                        _exceptionMessage.insert(_exceptionMessage.end(), (char)result[i]);
                    }
                    throw std::exception(_exceptionMessage.c_str());
                }
            }

            if (targetBufferSize != device->getCurrentBufferSizeSamples())
            {
                // die horribly
                throw std::exception("Can't set buffer size to target");
            }
        }
    }

    void NowSoundGraph::Initialize(
        int outputBinCount,
        float centralFrequency,
        int octaveDivisions,
        int centralBinIndex,
        int fftSize,
        float preRecordingDuration)
    {
        Log(L"Initialize(): start");

        _preRecordingDuration = preRecordingDuration;

        PrepareToChangeState(NowSoundGraphState::GraphUninitialized);

        // Before anything else, ensure that a MessageManager exists.
        // This is because, in this code, unless we do this, we get a crash with this stack:
        // >    NowSoundLib.dll!Timer::startTimer(int interval) Line 327    C++
        //    NowSoundLib.dll!DeviceChangeDetector::triggerAsyncDeviceChangeCallback() Line 108    C++
        //    NowSoundLib.dll!WasapiClasses::WASAPIAudioIODeviceType::ChangeNotificationClient::notify() Line 1517    C++
        //    NowSoundLib.dll!WasapiClasses::WASAPIAudioIODeviceType::ChangeNotificationClient::OnPropertyValueChanged(const wchar_t * __formal, const _tagpropertykey __formal) Line 1512    C++
        //    MMDevAPI.dll!00007ffdcb280f02()    Unknown
        // Not yet investigated....
        MessageManager::getInstance();

        // Initialize the ASIO device.
        {
            // Valid values for this on Surface Book are L"DirectSound" and L"Windows Audio".
            // Both seem to permit 96 sample buffering (!) but DirectSound seems to come up with
            // 16 bit audio sometimes... let's see if Windows Audio (AKA (AFAICT) WASAPI) causes
            // similar heartache.
            // ...it does not... but neither does it provide tolerable latency; shared mode minimum buffer
            // size of 144 samples (on FocusRite Scarlett 2i2), and exclusive mode is terribly crackly with
            // even a 480 sample buffer (and significantly worse at minimum 144).
            //
            // ASIO, on the other hand, is silky smooth and crackle-free at **16** sample buffer size.
            // So, so much for native Windows audio, they gave up the low latency game years ago and
            // forgot how to really play.
            String desiredDeviceType = L"ASIO";

            const OwnedArray<AudioIODeviceType>& deviceTypes = _audioDeviceManager.getAvailableDeviceTypes();

            // Set the audio device type.  Note that this actually winds up initializing one, which we don't want
            // but oh well.
            _audioDeviceManager.setCurrentAudioDeviceType(desiredDeviceType, /*treatAsChosenDevice*/ false);

            String result = _audioDeviceManager.initialiseWithDefaultDevices(2, 2);
            if (result.length() > 0)
            {
                ChangeState(NowSoundGraphState::GraphInError);
                return;
            }

            Log(L"Initialize(): end");
        }

        // Set up the ASIO device, clock, and audio allocator.
        NowSoundGraphInfo info;
        {
            // we expect ASIO
            Check(_audioDeviceManager.getCurrentAudioDeviceType() == L"ASIO");

            setBufferSize();

            info = Info();

            // insist on stereo float samples.  TODO: generalize channel count
            // For right now let's just make absolutely sure these values are all precisely as we intend every time.
            Check(_clock == nullptr);
            Check(info.ChannelCount == 2);
            Check(info.BitsPerSample == 32);

            _clock.reset(new NowSound::Clock(info.SampleRateHz,  info.ChannelCount));

            _tempo.reset(new NowSound::Tempo(
                MagicConstants::InitialBeatsPerMinute,
                MagicConstants::BeatsPerMeasure,
                info.SampleRateHz));

            _audioAllocator = std::unique_ptr<BufferAllocator<float>>(new BufferAllocator<float>(
                (int)(_clock->BytesPerSecond() * MagicConstants::AudioBufferSizeInSeconds.Value()),
                MagicConstants::InitialAudioBufferCount));
        }

        {
            _fftBinBounds.resize(outputBinCount);
            Check(_fftBinBounds.capacity() == outputBinCount);
            Check(_fftBinBounds.size() == outputBinCount);

            _fftSize = fftSize;

            // Initialize the bounds of the bins into which we collate FFT data.
            RosettaFFT::MakeBinBounds(
                _fftBinBounds,
                centralFrequency,
                octaveDivisions,
                outputBinCount,
                centralBinIndex,
                _clock->SampleRateHz(),
                fftSize);
        }

        // Set up the audio processor graph and its related components.
        {
            _audioProcessorPlayer.setProcessor(_audioProcessorGraph.get());
            _audioDeviceManager.addAudioCallback(&_audioProcessorPlayer);

            AudioProcessorGraph::AudioGraphIOProcessor* inputAudioProcessor =
                new AudioProcessorGraph::AudioGraphIOProcessor(AudioProcessorGraph::AudioGraphIOProcessor::IODeviceType::audioInputNode);
            AudioProcessorGraph::AudioGraphIOProcessor* outputAudioProcessor =
                new AudioProcessorGraph::AudioGraphIOProcessor(AudioProcessorGraph::AudioGraphIOProcessor::IODeviceType::audioOutputNode);

            MeasurementAudioProcessor* outputMixAudioProcessor = new MeasurementAudioProcessor(this, L"OutputMix");

            // TODO: don't hardcode stereo
            outputMixAudioProcessor->setPlayConfigDetails(2, 2, Info().SampleRateHz, Info().SamplesPerQuantum);

            // thank you to https://docs.juce.com/master/tutorial_audio_processor_graph.html
            _audioProcessorGraph.get()->setPlayConfigDetails(
                info.ChannelCount,
                info.ChannelCount,
                // call the JUCE method because it returns a higher precision than NowSoundInfo.SampleRate
                // TODO: consider making NowSoundInfo.SampleRate into a double
                _audioDeviceManager.getCurrentAudioDevice()->getCurrentSampleRate(),
                info.SamplesPerQuantum);

            // TBD: is double better?  Single (e.g. float32) definitely best for starters though
            _audioProcessorGraph.get()->setProcessingPrecision(AudioProcessor::singlePrecision);

            _audioProcessorGraph.get()->prepareToPlay(
                _audioDeviceManager.getCurrentAudioDevice()->getCurrentSampleRate(),
                info.SamplesPerQuantum);

            _audioInputNodePtr = _audioProcessorGraph.get()->addNode(inputAudioProcessor);
            _audioOutputNodePtr = _audioProcessorGraph.get()->addNode(outputAudioProcessor);
            _audioOutputMixNodePtr = _audioProcessorGraph.get()->addNode(outputMixAudioProcessor);

            for (int i = 0; i < info.ChannelCount; i++)
            {
                // handle this input channel by routing it through a SpatialAudioProcessor
                CreateNowSoundInputForChannel(i);

                // connect output mix to output
                Check(JuceGraph().addConnection({ { _audioOutputMixNodePtr->nodeID, i }, { _audioOutputNodePtr->nodeID, i } }));
            }
        }

        // and start everything!
        _audioDeviceManager.getCurrentAudioDevice()->start(&_audioProcessorPlayer);

        ChangeState(NowSoundGraphState::GraphRunning);
    }

    NowSoundGraphInfo NowSoundGraph::Info()
    {
        // TODO: verify not on audio graph thread
        AudioIODevice* device = _audioDeviceManager.getCurrentAudioDevice();

        auto activeInputChannels = device->getActiveInputChannels();
        auto activeOutputChannels = device->getActiveOutputChannels();
        auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
        auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
        if (maxInputChannels != maxOutputChannels)
        {
            throw "Don't yet support different numbers of input vs output channels";
        }

        auto availableBufferSizes = device->getAvailableBufferSizes();

        NowSoundGraphInfo graphInfo = CreateNowSoundGraphInfo(
            static_cast<int32_t>(device->getCurrentSampleRate()),
            maxInputChannels,
            device->getCurrentBitDepth(),
            device->getOutputLatencyInSamples(),
            device->getCurrentBufferSizeSamples());

        return graphInfo;
    }

    NowSoundSignalInfo NowSoundGraph::OutputSignalInfo()
    {
        Check(State() == NowSoundGraphState::GraphRunning);

        MeasurementAudioProcessor* outputMixProcessor = dynamic_cast<MeasurementAudioProcessor*>(
            _audioOutputMixNodePtr->getProcessor());

        return outputMixProcessor->SignalInfo();
    }

#ifdef INPUT_DEVICE_SELECTION
    void NowSoundGraph::InputDeviceId(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity)
    {
        // wcsncpy_s(wcharBuffer, bufferCapacity, _inputDeviceInfos[deviceIndex].Id().c_str(), _TRUNCATE);
    }

    void NowSoundGraph::InputDeviceName(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity)
    {
        // wcsncpy_s(wcharBuffer, bufferCapacity, _inputDeviceInfos[deviceIndex].Name().c_str(), _TRUNCATE);
    }

    void NowSoundGraph::InitializeDeviceInputs(int deviceIndex)
    {
        Check(State() == NowSoundGraphState::GraphInitialized);

        _inputDeviceIndicesToInitialize.push_back(deviceIndex);
    }
#endif

    const std::vector<RosettaFFT::FrequencyBinBounds>* NowSoundGraph::BinBounds() const { return &_fftBinBounds; }

    int NowSoundGraph::FftSize() const { return _fftSize; }

    ContinuousDuration<Second> NowSoundGraph::PreRecordingDuration() const { return _preRecordingDuration; }

    void NowSoundGraph::CreateNowSoundInputForChannel(int channel)
    {
        AudioInputId id(static_cast<AudioInputId>((int)(channel + 1)));
        NowSoundInputAudioProcessor* inputProcessor = new NowSoundInputAudioProcessor(
            this,
            id,
            _audioAllocator.get(),
            channel);

        AddInputNodeToJuceGraph(inputProcessor, channel);

        _audioInputs.push_back(inputProcessor);
    }

    NowSoundTimeInfo NowSoundGraph::TimeInfo()
    {
        // TODO: verify not on audio graph thread

        Check(_audioGraphState > NowSoundGraphState::GraphInError);

        Time<AudioSample> now = _clock->Now();
        ContinuousDuration<Beat> durationBeats = _tempo->TimeToBeats(now.AsContinuous());
        int64_t completeBeats = (int64_t)durationBeats.Value();
        int32_t beatsPerMeasure = _tempo->BeatsPerMeasure();
        int64_t completeMeasures = completeBeats / beatsPerMeasure;

        NowSoundTimeInfo timeInfo = CreateNowSoundTimeInfo(
            now.Value(),
            durationBeats.Value(),
            _tempo->BeatsPerMinute(),
            _tempo->BeatsPerMeasure(),
            durationBeats.Value() - (completeMeasures * beatsPerMeasure));

        return timeInfo;
    }

    Tempo* NowSoundGraph::Tempo() const
    {
        return _tempo.get();
    }

    Clock* NowSoundGraph::Clock() const
    {
        return _clock.get();
    }

    void NowSoundGraph::SetTempo(float beatsPerMinute, int beatsPerMeasure)
    {
        _tempo.reset(new NowSound::Tempo(beatsPerMinute, beatsPerMeasure, _clock->SampleRateHz()));

        {
            std::wstringstream wstr{};
            wstr << L"Set tempo bpm to " << beatsPerMinute << L"; tempo bpm is now " << _tempo->BeatsPerMinute();
            NowSoundGraph::Instance()->Log(wstr.str());
        }
    }

    AudioProcessorGraph::Node::Ptr NowSoundGraph::GetNodePtr(BaseAudioProcessor* audioProcessor)
    {
        AudioProcessorGraph::NodeID nodeId = audioProcessor->NodeId();
        // must have been set
        Check(nodeId != AudioProcessorGraph::NodeID{});

        AudioProcessorGraph::Node::Ptr nodePtr = JuceGraph().getNodeForId(nodeId);
        return nodePtr;
    }

    NowSoundInputAudioProcessor* NowSoundGraph::Input(AudioInputId audioInputId)
    {
        Check(_audioGraphState > NowSoundGraphState::GraphInError);

        Check(audioInputId > AudioInputId::AudioInputUndefined);
        // Input IDs are one-based
        Check((audioInputId - 1) < _audioInputs.size());

        return _audioInputs[((int)audioInputId) - 1];
    }

    float NowSoundGraph::InputPan(AudioInputId id)
    {
        return Input(id)->Pan();
    }

    void NowSoundGraph::InputPan(AudioInputId id, float pan)
    {
        Input(id)->Pan(pan);
    }

    TrackId NowSoundGraph::CreateRecordingTrackAsync(AudioInputId audioInputId)
    {
        // TODO: verify not on audio graph thread
        Check(_audioGraphState == NowSoundGraphState::GraphRunning);

        // by construction this will be greater than TrackId::Undefined
        TrackId id = (TrackId)((int)_nextTrackId + 1);
        _nextTrackId = id;

        NowSoundTrackAudioProcessor* newTrack = Input(audioInputId)->CreateRecordingTrack(id);

        _tracks.insert(std::pair<TrackId, NowSoundTrackAudioProcessor*>{id, newTrack});

        // convert from audio input numbering (1-based) to channel id (0-based)
        AddRecordingNodeToJuceGraph(newTrack, audioInputId);

        return id;
    }

    TrackId NowSoundGraph::CopyLoopingTrack(TrackId trackId)
    {
        // TODO: verify not on audio graph thread
        Check(_audioGraphState == NowSoundGraphState::GraphRunning);
        Check(trackId != TrackId::TrackIdUndefined);

        // get new track id for this track
        TrackId id = (TrackId)((int)_nextTrackId + 1);
        _nextTrackId = id;

        NowSoundTrackAudioProcessor* newTrack = new NowSoundTrackAudioProcessor(id, Track(trackId));

        _tracks.insert(std::pair<TrackId, NowSoundTrackAudioProcessor*>{id, newTrack});

        // we only give this a variable name for debugging purposes
        AudioProcessorGraph::NodeID newNodeId = AddNodeToJuceGraph(newTrack, NodeType::Looping);

        LogConnections();

        return id;
    }

    void NowSoundGraph::DeleteTrack(TrackId trackId)
    {
        Check(trackId >= TrackId::TrackIdUndefined && _tracks.find(trackId) != _tracks.end());

        // remove the owning Node from the graph
        NowSoundTrackAudioProcessor* track = _tracks.at(trackId);

        // wipe the weak reference first (it will be destructed after the Delete() anyway)
        _tracks.erase(trackId);

        // delete the track; this drops all nodes it manages from the JUCE graph, including the track object itself
        track->Delete();

        // this is an async update (if we weren't running JUCE in such a hacky way, we wouldn't need to know this)
        JuceGraphChanged();
    }

    void NowSoundGraph::AddPluginSearchPath(LPWSTR wcharBuffer, int32_t bufferCapacity)
    {
        String path(wcharBuffer);
        _audioPluginSearchPaths.push_back(path);
    }

    bool NowSoundGraph::SearchPluginsSynchronously()
    {
        _audioPluginFormatManager.addDefaultFormats();

        AudioPluginFormat* vstFormat = nullptr;
        for (int i = 0; i < _audioPluginFormatManager.getNumFormats(); i++)
        {
            vstFormat = _audioPluginFormatManager.getFormat(i);
            if (vstFormat->getName().startsWith(String(L"VST")))
            {
                ScanPluginFormat(vstFormat);
            }
        }

        // and that's it!
        return true;
    }

    void NowSoundGraph::ScanPluginFormat(juce::AudioPluginFormat* vstFormat)
    {
        Check(vstFormat != nullptr);

        FileSearchPath fileSearchPath{};
        for (const String& path : _audioPluginSearchPaths)
        {
            fileSearchPath.add(path);
        }

        PluginDirectoryScanner scanner{
            _knownPluginList,
            *vstFormat,
            fileSearchPath,
            true,
            File(),
            false }; // turns out "allowAsync" parameter is a no-op for VST2 plugins

        String pluginBeingScanned{};
        // loop over all files synchronously, this is a bad experience if the path is huge so don't ever do that
        while (scanner.scanNextFile(/*dontRescanIfAlreadyInList*/true, pluginBeingScanned)) {};

        // now initialize _loadedPluginPrograms with an empty vector per plugin
        for (int i = 0; i < _knownPluginList.getNumTypes(); i++)
        {
            _loadedPluginPrograms.push_back(std::vector<PluginProgram>{});
        }
    }

    int NowSoundGraph::PluginCount()
    {
        return _knownPluginList.getNumTypes();
    }

    void NowSoundGraph::PluginName(PluginId pluginId, LPWSTR wcharBuffer, int32_t bufferCapacity)
    {
        PluginDescription* desc = _knownPluginList.getType(((int)pluginId) - 1);
        const String& name = desc->name;

        wcsncpy_s(wcharBuffer, (size_t)bufferCapacity, name.getCharPointer(), name.length());
    }

    struct FileNameComparer
    {
        int compareElements(File file1, File file2)
        {
            return file1.getFileName().compare(file2.getFileName());
        }
    };

    bool NowSoundGraph::LoadPluginPrograms(PluginId pluginId, LPWSTR pathnameBuffer)
    {
        // Verify that pathnameBuffer exists
        String pathname{ pathnameBuffer };
        File path{ pathname };
        if (!path.isDirectory())
        {
            Log(L"NowSoundGraph::LoadPluginPrograms(): path is not directory");
            std::wstring pathNameWString{ pathnameBuffer };
            Log(pathNameWString);
            return false;
        }

        std::vector<PluginProgram> programs{};

        // Iterate over all ".state" files in path
        auto childFiles = path.findChildFiles(File::TypesOfFileToFind::findFiles, /*searchRecursively*/ false, "*.state");
        FileNameComparer comparer{};
        childFiles.sort(comparer, /*retainOrderOfEquivalentItems*/ false);
        for (File file : childFiles)
        {
            String programName = file.getFileNameWithoutExtension();
            Log(L"NowSoundGraph::LoadPluginPrograms(): loaded program");
            std::wstring wstr{ programName.getCharPointer() };
            Log(wstr);
            FileInputStream finStream(file);
            int32_t size = finStream.readInt();
            MemoryBlock state;
            state.ensureSize(size);
            finStream.read(state.getData(), size);
            programs.push_back(PluginProgram{ state, programName });
        }

        _loadedPluginPrograms.emplace(_loadedPluginPrograms.begin() + ((int)pluginId - 1), std::move(programs));

        return true;
    }

    int32_t NowSoundGraph::PluginProgramCount(PluginId pluginId)
    {
        return static_cast<int32_t>(_loadedPluginPrograms[(int)pluginId - 1].size());
    }

    void NowSoundGraph::PluginProgramName(PluginId pluginId, ProgramId programId, LPWSTR wcharBuffer, int32_t bufferCapacity)
    {
        const String& name = _loadedPluginPrograms[(int)pluginId - 1][(int)programId - 1].Name();

        wcsncpy_s(wcharBuffer, (size_t)bufferCapacity, name.getCharPointer(), name.length());
    }

    void NowSoundGraph::AddInputNodeToJuceGraph(SpatialAudioProcessor* newProcessor, int inputChannel)
    {
        AudioProcessorGraph::NodeID newNodeId = AddNodeToJuceGraph(newProcessor, NodeType::Input);

        // Input connection (only one, from designated channel to new processor's channel 0)
        Check(JuceGraph().addConnection({ { _audioInputNodePtr->nodeID, inputChannel }, { newNodeId, 0 } }));

        {
            std::wstringstream wstr{};
            wstr << L"NowSoundGraph::AddInputNodeToJuceGraph(channel #" << inputChannel << L") = " << newNodeId.uid;
            NowSoundGraph::Instance()->Log(wstr.str());
        }
    }

    void NowSoundGraph::AddRecordingNodeToJuceGraph(SpatialAudioProcessor* newProcessor, AudioInputId audioInputId)
    {
        AudioProcessorGraph::NodeID newNodeId = AddNodeToJuceGraph(newProcessor, NodeType::Recording);

        // Input connections (one per output channel); consume the *pre-effect* input
        Check(JuceGraph().addConnection({ { Input(audioInputId)->NodeId(), 0 }, { newNodeId, 0 } }));
        Check(JuceGraph().addConnection({ { Input(audioInputId)->NodeId(), 1 }, { newNodeId, 1 } }));

        {
            std::wstringstream wstr{};
            wstr << L"NowSoundGraph::AddRecordingNodeToJuceGraph(input #" << audioInputId << L") = " << newNodeId.uid;
            NowSoundGraph::Instance()->Log(wstr.str());
        }
    }

    AudioProcessorGraph::NodeID NowSoundGraph::AddNodeToJuceGraph(SpatialAudioProcessor* newProcessor, NodeType nodeType)
    {
        Check(nodeType != NodeType::Undefined);

        // set play config details BEFORE making connections to the graph
        // otherwise addConnection doesn't think the new node has any connections
        int inputConnections;
        switch (nodeType) {
            case NodeType::Input: inputConnections = 1; break;
            case NodeType::Recording: inputConnections = 2; break;
            case NodeType::Looping: inputConnections = 0; break;
        }

        newProcessor->setPlayConfigDetails(inputConnections, 2, Info().SampleRateHz, Info().SamplesPerQuantum);
        newProcessor->OutputProcessor()->setPlayConfigDetails(2, 2, Info().SampleRateHz, Info().SamplesPerQuantum);

        AudioProcessorGraph::Node::Ptr inputNode = JuceGraph().addNode(newProcessor);
        AudioProcessorGraph::Node::Ptr outputNode = JuceGraph().addNode(newProcessor->OutputProcessor());
        newProcessor->SetNodeIds(inputNode->nodeID, outputNode->nodeID);

        // Output connections
        // TODO: enumerate based on actual graph count... for the moment, stereo only
        Check(JuceGraph().addConnection({ { outputNode->nodeID, 0 }, { _audioOutputMixNodePtr->nodeID, 0 } }));
        Check(JuceGraph().addConnection({ { outputNode->nodeID, 1 }, { _audioOutputMixNodePtr->nodeID, 1 } }));

        // this is an async update (if we weren't running JUCE in such a hacky way, we wouldn't need to know this)
        JuceGraphChanged();

        return inputNode->nodeID;
    }

    void NowSoundGraph::LogConnections()
    {
        uint32 maxConnNodeId = 0;
        for (auto conn : JuceGraph().getConnections())
        {
            std::wstringstream wstr;
            auto sourceId = conn.source.nodeID;
            auto destId = conn.destination.nodeID;
            wstr << L"Connection: " << sourceId.uid << L"/" << conn.source.channelIndex
                << " (" << JuceGraph().getNodeForId(sourceId)->getProcessor()->getName() << ")"
                << " -> " << destId.uid << L"/" << conn.destination.channelIndex
                << " (" << JuceGraph().getNodeForId(destId)->getProcessor()->getName() << ")"
                ;
            Log(wstr.str());

            if (conn.source.nodeID.uid > maxConnNodeId)
            {
                maxConnNodeId = conn.source.nodeID.uid;
            }
            if (conn.destination.nodeID.uid > maxConnNodeId)
            {
                maxConnNodeId = conn.destination.nodeID.uid;
            }
        }
        for (uint32 i = 1; i <= maxConnNodeId; i++)
        {
            LogNode((AudioProcessorGraph::NodeID)i);
        }
    }

    void NowSoundGraph::LogNode(AudioProcessorGraph::NodeID nodeId)
    {
        const AudioProcessorGraph::Node* nodePtr = JuceGraph().getNodeForId(nodeId);

        std::wstringstream wstr;

        if (nodePtr != nullptr)
        {
            if (nodePtr->getProcessor() != nullptr)
            {
                String name{ nodePtr->getProcessor()->getName() };
                bool isBypassed{ nodePtr->isBypassed() };
                bool isSuspended{ nodePtr->getProcessor()->isSuspended() };
                int latencySamples{ nodePtr->getProcessor()->getLatencySamples() };
                double sampleRate{ nodePtr->getProcessor()->getSampleRate() };
                int totalNumInputChannels{ nodePtr->getProcessor()->getTotalNumInputChannels() };
                int totalNumOutputChannels{ nodePtr->getProcessor()->getTotalNumOutputChannels() };

                wstr << L"Node #" << nodeId.uid << L": "
                    << name
                    << L": bypassed " << isBypassed
                    << L", suspended " << isSuspended
                    << L", latencySamples " << latencySamples
                    << L", sampleRate " << sampleRate
                    << L", totalNumInputChannels " << totalNumInputChannels
                    << L", totalNumOutputChannels " << totalNumOutputChannels;
            }
            else
            {
                wstr << L"Node #" << nodeId.uid << L": no processor";
            }
            Log(wstr.str());
        }

        // ...else, if no such node, don't even mention it
    }

    void NowSoundGraph::JuceGraphChanged()
    {
        // This would seem to be unnecessary, but we need to ensure we do not lose a call to this method
        // in the event that the message tick thread is calling WasJuceGraphChanged() concurrently with this,
        // resulting in the update here being lost (because WasJuceGraphChanged() resets this flag).
        std::lock_guard<std::mutex> guard(_juceGraphChangedMutex);
        _juceGraphChanged = true;
    }

    bool NowSoundGraph::WasJuceGraphChanged()
    {
        // This would seem to be unnecessary, but we need to ensure we do not lose a call to this method
        // in the event that the message tick thread is calling WasAsyncUpdate() concurrently with this,
        // resulting in the update here being lost (because WasAsyncUpdate() resets this flag).
        std::lock_guard<std::mutex> guard(_juceGraphChangedMutex);
        bool result = _juceGraphChanged;
        _juceGraphChanged = false;
        return result;
    }

    AudioProcessor* NowSoundGraph::CreatePluginProcessor(PluginId pluginId, ProgramId programId)
    {
        PluginDescription* desc = _knownPluginList.getType(((int)pluginId) - 1);
        String errorMessage;
        AudioProcessor* instance = _audioPluginFormatManager.createPluginInstance(
            *desc,
            Info().SampleRateHz,
            Info().SamplesPerQuantum,
            errorMessage);

        Check(errorMessage == L"");

        // and set the state according to the requested program
        const MemoryBlock& state = _loadedPluginPrograms[((int)pluginId) - 1][((int)programId) - 1].State();
        instance->setStateInformation(state.getData(), static_cast<int>(state.getSize()));

        return instance;
    }

    void NowSoundGraph::MessageTick()
    {
        if (WasJuceGraphChanged())
        {
            // call the JUCE graph's handleAsyncUpdate() method directly.
            _audioProcessorGraph.get()->handleAsyncUpdate();
        }
    }

    // Start recording to the given filename (WAV format); if already recording, this is ignored.
    void NowSoundGraph::StartRecording(LPWSTR fileName, int32_t fileNameLength)
    {
        // pass this request along to the output node
        Check(State() == NowSoundGraphState::GraphRunning);

        // The output mix is what we want to record, and conveniently we already have a MeasurementAudioProcessor
        // tracking the final mix.
        MeasurementAudioProcessor* outputMixProcessor = dynamic_cast<MeasurementAudioProcessor*>(
            _audioOutputMixNodePtr->getProcessor());

        outputMixProcessor->StartRecording(fileName, fileNameLength);
    }

    // Stop recording and close the file; if not recording, this is ignored.
    void NowSoundGraph::StopRecording()
    {
        MeasurementAudioProcessor* outputMixProcessor = dynamic_cast<MeasurementAudioProcessor*>(
            _audioOutputMixNodePtr->getProcessor());

        outputMixProcessor->StopRecording();
    }

    void NowSoundGraph::ShutdownInstance()
    {
        // SHUT. DOWN. EVERYTHING
        s_instance->Shutdown();

        // and then destruct the singleton
        s_instance = nullptr;
    }

    // instance shutdown method for instance internal state
    void NowSoundGraph::Shutdown()
    {
        _audioDeviceManager.removeAllChangeListeners();
        _audioDeviceManager.closeAudioDevice();
        _audioDeviceManager.removeAudioCallback(&_audioProcessorPlayer);

        // clear the graph before we destruct this object (which will kill the allocator, which will
        // break stream teardown)
        _audioProcessorGraph.get()->clear();

        // and in fact, drop it now, so by the time we get to destructor, it has completed its shutdown
        _audioProcessorGraph.release();
    }
}
