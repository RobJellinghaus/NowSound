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

using namespace concurrency;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace winrt::Windows::Foundation;

namespace NowSound
{
	TimeSpan timeSpanFromSeconds(int seconds)
	{
		// TimeSpan is in 100ns units
		return TimeSpan(seconds * Clock::TicksPerSecond);
	}

	std::unique_ptr<NowSoundGraph> NowSoundGraph::s_instance{ nullptr };

	NowSoundGraph* NowSoundGraph::Instance() { return s_instance.get(); }

	void NowSoundGraph::InitializeInstance()
	{
		std::unique_ptr<NowSoundGraph> temp{ new NowSoundGraph() };
		s_instance = std::move(temp);
		s_instance.get()->Initialize();
	}

	NowSoundGraph::NowSoundGraph()
		: // _audioGraph{ nullptr },
		_audioGraphState{ NowSoundGraphState::GraphUninitialized },
		_audioDeviceManager{},
		// _deviceOutputNode{ nullptr },
		_audioAllocator{ nullptr },
		_trackId{ TrackId::TrackIdUndefined },
		_nextAudioInputId{ AudioInputId::AudioInputUndefined },
		// JUCETODO: _inputDeviceIndicesToInitialize{},
		_audioInputs{ },
		_changingState{ false },
		_fftBinBounds{},
		_fftSize{ -1 },
		_stateMutex{}
	{ }

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

    juce::AudioProcessorGraph& NowSoundGraph::JuceGraph()
    {
        return _audioProcessorGraph;
    }

    void NowSoundGraph::setBufferSizeToMinimum()
    {
        // Set buffer size to minimum available on current device
        auto* device = _audioDeviceManager.getCurrentAudioDevice();

        auto bufferRate = device->getCurrentSampleRate();
        auto bufferSize = device->getCurrentBufferSizeSamples();

        const bool setMinBufferSize = true;
        if (setMinBufferSize)
        {
            auto bufferSizes = device->getAvailableBufferSizes();
            int minBufferSize = bufferSizes[0];

            AudioDeviceManager::AudioDeviceSetup setup;
            _audioDeviceManager.getAudioDeviceSetup(setup);

            if (setup.bufferSize > minBufferSize)
            {
                setup.bufferSize = minBufferSize;
                String result = _audioDeviceManager.setAudioDeviceSetup(setup, false);
                if (result.length() > 0)
                {
                    throw std::exception(result.getCharPointer());
                }
            }

            if (minBufferSize != device->getCurrentBufferSizeSamples())
            {
                // die horribly
                throw std::exception("Can't set buffer size to minimum");
            }
        }
    }

	void NowSoundGraph::Initialize()
	{
		PrepareToChangeState(NowSoundGraphState::GraphUninitialized);

		// Before anything else, ensure that a MessageManager exists.
		// This is because, in this code, unless we do this, we get a crash with this stack:
		// >	NowSoundLib.dll!juce::Timer::startTimer(int interval) Line 327	C++
		//	NowSoundLib.dll!juce::DeviceChangeDetector::triggerAsyncDeviceChangeCallback() Line 108	C++
		//	NowSoundLib.dll!juce::WasapiClasses::WASAPIAudioIODeviceType::ChangeNotificationClient::notify() Line 1517	C++
		//	NowSoundLib.dll!juce::WasapiClasses::WASAPIAudioIODeviceType::ChangeNotificationClient::OnPropertyValueChanged(const wchar_t * __formal, const _tagpropertykey __formal) Line 1512	C++
		//	MMDevAPI.dll!00007ffdcb280f02()	Unknown
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
            juce::String desiredDeviceType = L"ASIO";

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
        }

        // Set up the ASIO device, clock, and audio allocator.
        NowSoundGraphInfo info;
        {            
            // we expect ASIO
            Check(_audioDeviceManager.getCurrentAudioDeviceType() == L"ASIO");

            setBufferSizeToMinimum();

            info = Info();

            // insist on stereo float samples.  TODO: generalize channel count
            // For right now let's just make absolutely sure these values are all precisely as we intend every time.
            Check(info.ChannelCount == 2);
            Check(info.BitsPerSample == 32);

            Clock::Initialize(
                info.SampleRateHz,
                info.ChannelCount,
                MagicConstants::InitialBeatsPerMinute,
                MagicConstants::BeatsPerMeasure);

            _audioAllocator = std::unique_ptr<BufferAllocator<float>>(new BufferAllocator<float>(
                (int)(Clock::Instance().BytesPerSecond() * MagicConstants::AudioBufferSizeInSeconds.Value()),
                MagicConstants::InitialAudioBufferCount));
        }

        // Set up the audio processor graph and its related components.
        {
            _audioProcessorPlayer.setProcessor(&_audioProcessorGraph);
            _audioDeviceManager.addAudioCallback(&_audioProcessorPlayer);

            juce::AudioProcessorGraph::AudioGraphIOProcessor* inputAudioProcessor =
                new juce::AudioProcessorGraph::AudioGraphIOProcessor(juce::AudioProcessorGraph::AudioGraphIOProcessor::IODeviceType::audioInputNode);
            juce::AudioProcessorGraph::AudioGraphIOProcessor* outputAudioProcessor =
                new juce::AudioProcessorGraph::AudioGraphIOProcessor(juce::AudioProcessorGraph::AudioGraphIOProcessor::IODeviceType::audioOutputNode);

            // thank you to https://docs.juce.com/master/tutorial_audio_processor_graph.html
            _audioProcessorGraph.setPlayConfigDetails(
                info.ChannelCount,
                info.ChannelCount,
                // call the JUCE method because it returns a higher precision than NowSoundInfo.SampleRate
                // TODO: consider making NowSoundInfo.SampleRate into a double
                _audioDeviceManager.getCurrentAudioDevice()->getCurrentSampleRate(),
                info.SamplesPerQuantum);

            // TBD: is double better?  Single (e.g. float32) definitely best for starters though
            _audioProcessorGraph.setProcessingPrecision(AudioProcessor::singlePrecision);

            _audioProcessorGraph.prepareToPlay(
                _audioDeviceManager.getCurrentAudioDevice()->getCurrentSampleRate(),
                info.SamplesPerQuantum);

            _audioInputNodePtr = _audioProcessorGraph.addNode(inputAudioProcessor);
            _audioOutputNodePtr = _audioProcessorGraph.addNode(outputAudioProcessor);
            for (int i = 0; i < info.ChannelCount; i++)
            {
                CreateInputDeviceForChannel(i);

                // DIRECT ROUTING (commented out now because we route the inputs separately, theoretically at least):
                // _audioProcessorGraph.addConnection({ { _audioInputNodePtr->nodeID, i }, { _audioOutputNodePtr->nodeID, i } });
            }
        }

        // and start everything!
		_audioDeviceManager.getCurrentAudioDevice()->start(&_audioProcessorPlayer);

		ChangeState(NowSoundGraphState::GraphRunning);
	}

	NowSoundGraphInfo NowSoundGraph::Info()
	{
		// TODO: verify not on audio graph thread
		juce::AudioIODevice* device = _audioDeviceManager.getCurrentAudioDevice();

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
			device->getCurrentSampleRate(),
			maxInputChannels,
			device->getCurrentBitDepth(),
			device->getOutputLatencyInSamples(),
			device->getCurrentBufferSizeSamples());

		return graphInfo;
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

	void NowSoundGraph::InitializeFFT(
		int outputBinCount,
		float centralFrequency,
		int octaveDivisions,
		int centralBinIndex,
		int fftSize)
	{
		_fftBinBounds.resize(outputBinCount);
		_fftSize = fftSize;

		// Initialize the bounds of the bins into which we collate FFT data.
		RosettaFFT::MakeBinBounds(
			_fftBinBounds,
			centralFrequency,
			octaveDivisions,
			outputBinCount,
			centralBinIndex,
			Clock::Instance().SampleRateHz(),
			fftSize);
	}

	const std::vector<RosettaFFT::FrequencyBinBounds>* NowSoundGraph::BinBounds() const { return &_fftBinBounds; }

	int NowSoundGraph::FftSize() const { return _fftSize; }

	void NowSoundGraph::CreateInputDeviceForChannel(int channel)
	{
		AudioInputId nextAudioInputId(static_cast<AudioInputId>((int)(_audioInputs.size() + 1)));
		juce::AudioProcessorGraph::Node::Ptr newPtr = _audioProcessorGraph.addNode(
            new NowSoundInputAudioProcessor(
			    this,
			    nextAudioInputId,
			    _audioAllocator.get(),
			    channel));

        AddNodeToJuceGraph(newPtr, channel);

		_audioInputs.emplace_back(newPtr);
	}

	NowSoundTimeInfo NowSoundGraph::TimeInfo()
	{
		// TODO: verify not on audio graph thread

		Check(_audioGraphState > NowSoundGraphState::GraphInError);

		Time<AudioSample> now = Clock::Instance().Now();
		ContinuousDuration<Beat> durationBeats = Clock::Instance().TimeToBeats(now);
		int64_t completeBeats = (int64_t)durationBeats.Value();

		NowSoundTimeInfo timeInfo = CreateNowSoundTimeInfo(
			// JUCETODO: (int32_t)_audioInputs.size(),
			now.Value(),
			durationBeats.Value(),
			Clock::Instance().BeatsPerMinute(),
			(float)(completeBeats % Clock::Instance().BeatsPerMeasure()));

		return timeInfo;
	}

    NowSoundInputAudioProcessor* NowSoundGraph::Input(AudioInputId audioInputId)
    {
        Check(_audioGraphState > NowSoundGraphState::GraphInError);

        Check(audioInputId > AudioInputId::AudioInputUndefined);
        // Input IDs are one-based
        Check((audioInputId - 1) < _audioInputs.size());

        juce::AudioProcessorGraph::Node::Ptr& input = _audioInputs[(int)audioInputId - 1];

        // ensure non-null -- should be true by construction, but, you know, bugs
        Check(input.get());

        return static_cast<NowSoundInputAudioProcessor*>(input->getProcessor());
    }

	NowSoundInputInfo NowSoundGraph::InputInfo(AudioInputId audioInputId)
	{
		return Input(audioInputId)->Info();
	}

	TrackId NowSoundGraph::CreateRecordingTrackAsync(AudioInputId audioInputId)
    {
        // TODO: verify not on audio graph thread
        Check(_audioGraphState == NowSoundGraphState::GraphRunning);

        // by construction this will be greater than TrackId::Undefined
        TrackId id = (TrackId)((int)_trackId + 1);
        _trackId = id;

        juce::AudioProcessorGraph::Node::Ptr newTrackPtr = Input(audioInputId)->CreateRecordingTrack(id);

        // convert from audio input numbering (1-based) to channel id (0-based)
        AddNodeToJuceGraph(newTrackPtr, audioInputId - 1);

		return id;
    }

    void NowSoundGraph::PlayUserSelectedSoundFileAsyncImpl()
    {
#if JUCETODO
        // This must be called on the UI thread.
        FileOpenPicker picker;
        picker.SuggestedStartLocation(PickerLocationId::MusicLibrary);
        picker.FileTypeFilter().Append(L".wav");
        StorageFile file = co_await picker.PickSingleFileAsync();

        if (!file)
        {
            Check(false);
            return;
        }

        CreateAudioFileInputNodeResult fileInputResult = co_await _audioGraph.CreateFileInputNodeAsync(file);
        if (AudioFileNodeCreationStatus::Success != fileInputResult.Status())
        {
            // Cannot read input file
            Check(false);
            return;
        }

        AudioFileInputNode fileInput = fileInputResult.FileInputNode();

        if (fileInput.Duration() <= timeSpanFromSeconds(3))
        {
            // Imported file is too short
            Check(false);
            return;
        }

        fileInput.AddOutgoingConnection(_deviceOutputNode);
        fileInput.Start();
#endif
    }

    void NowSoundGraph::PlayUserSelectedSoundFileAsync()
    {
        PlayUserSelectedSoundFileAsyncImpl();
    }

    void NowSoundGraph::ShutdownInstance()
    {
		// drop the singleton
		s_instance = nullptr;
    }

    void NowSoundGraph::AddNodeToJuceGraph(juce::AudioProcessorGraph::Node::Ptr newNode, int inputChannel)
    {
        // set play config details BEFORE making connections to the graph
        // otherwise addConnection doesn't think the new node has any connections
        newNode->getProcessor()->setPlayConfigDetails(1, 2, Info().SampleRateHz, Info().SamplesPerQuantum);

        // Input connection (one)
        Check(JuceGraph().addConnection({ { _audioInputNodePtr->nodeID, inputChannel }, { newNode->nodeID, 0 } }));

        // Output connections
        // TODO: enumerate based on actual graph count... for the moment, stereo only
        Check(JuceGraph().addConnection({ { newNode->nodeID, 0 }, { _audioOutputNodePtr->nodeID, 0 } }));
        Check(JuceGraph().addConnection({ { newNode->nodeID, 1 }, { _audioOutputNodePtr->nodeID, 1 } }));
    }
}
