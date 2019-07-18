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

	void NowSoundGraph::InitializeInstance()
	{
		std::unique_ptr<NowSoundGraph> temp{ new NowSoundGraph() };
		s_instance = std::move(temp);
		s_instance.get()->Initialize();
	}

	NowSoundGraph::NowSoundGraph() :
		_audioGraphState{ NowSoundGraphState::GraphUninitialized },
		_audioDeviceManager{},
		_audioAllocator{ nullptr },
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
		_asyncUpdate{},
		_asyncUpdateMutex{},
		_audioPluginSearchPaths{},
		_knownPluginList{},
		_audioPluginFormatManager{}
	{
		_logMessages.reserve(s_logMessageCapacity);
		Check(_logMessages.size() == 0);
	}

	void NowSoundGraph::AddTrack(TrackId id, juce::AudioProcessorGraph::Node::Ptr track)
	{
		// we want to insert the track by copy, since it's ref-counted
		_tracks.insert(std::pair<TrackId, juce::AudioProcessorGraph::Node::Ptr>{id, track});

		// this is an async update
		AsyncUpdate();
	}

	NowSoundTrackAudioProcessor* NowSoundGraph::Track(TrackId id)
	{
		// NOTE THAT THIS PATTERN DOES NOT LOCK THE _tracks COLLECTION IN ANY WAY.
		// The only way this will be correct is if all modifications to s_tracks happen only as a result of
		// non-concurrent, serialized external calls to NowSoundTrackAPI.
		Check(id > TrackId::TrackIdUndefined);
		NowSoundTrackAudioProcessor* value = static_cast<NowSoundTrackAudioProcessor*>(_tracks.at(id)->getProcessor());
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
		info.LogMessageCount = _logMessages.size();
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

	juce::AudioProcessorGraph& NowSoundGraph::JuceGraph()
	{
		return _audioProcessorGraph;
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

	void NowSoundGraph::Initialize()
	{
		Log(L"Initialize(): start");

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
			Check(!Clock::IsInitialized());
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

			MeasurementAudioProcessor* outputMixAudioProcessor = new MeasurementAudioProcessor(this, L"OutputMix");

			// TODO: don't hardcode stereo
			outputMixAudioProcessor->setPlayConfigDetails(2, 2, Info().SampleRateHz, Info().SamplesPerQuantum);

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
			_audioOutputMixNodePtr = _audioProcessorGraph.addNode(outputMixAudioProcessor);

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

	void NowSoundGraph::InitializeFFT(
		int outputBinCount,
		float centralFrequency,
		int octaveDivisions,
		int centralBinIndex,
		int fftSize)
	{
		_fftBinBounds.resize(outputBinCount);

		Check(_fftBinBounds.capacity() == outputBinCount);
		Check(_fftBinBounds.capacity() == outputBinCount);
		Check(_fftBinBounds.capacity() == outputBinCount);
		Check(_fftBinBounds.capacity() == outputBinCount);
		Check(_fftBinBounds.capacity() == outputBinCount);
		Check(_fftBinBounds.capacity() == outputBinCount);

		Check(_fftBinBounds.size() == outputBinCount);
		Check(_fftBinBounds.size() == outputBinCount);
		Check(_fftBinBounds.size() == outputBinCount);
		Check(_fftBinBounds.size() == outputBinCount);
		Check(_fftBinBounds.size() == outputBinCount);
		Check(_fftBinBounds.size() == outputBinCount);

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

	void NowSoundGraph::CreateNowSoundInputForChannel(int channel)
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
		TrackId id = (TrackId)((int)_nextTrackId + 1);
		_nextTrackId = id;

		juce::AudioProcessorGraph::Node::Ptr newTrackPtr = Input(audioInputId)->CreateRecordingTrack(id);

		// convert from audio input numbering (1-based) to channel id (0-based)
		AddNodeToJuceGraph(newTrackPtr, audioInputId - 1);

		return id;
	}

	void NowSoundGraph::DeleteTrack(TrackId trackId)
	{
		Check(trackId >= TrackId::TrackIdUndefined && trackId <= _tracks.size());

		// remove the Node
		juce::AudioProcessorGraph::Node::Ptr& nodePtr = _tracks.at(trackId);
		JuceGraph().removeNode(nodePtr.get());

		// place a null pointer... this *should* cause the track to be deleted
		_tracks[trackId] = nullptr;

		// this is an async update (if we weren't running JUCE in such a hacky way, we wouldn't need to know this)
		AsyncUpdate();
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

	void NowSoundGraph::AddPluginSearchPath(LPWSTR wcharBuffer, int32_t bufferCapacity)
	{
		juce::String path(wcharBuffer);
		_audioPluginSearchPaths.push_back(path);
	}

	bool NowSoundGraph::SearchPluginsSynchronously()
	{
		_audioPluginFormatManager.addDefaultFormats();

		AudioPluginFormat* vstFormat = nullptr;
		for (int i = 0; i < _audioPluginFormatManager.getNumFormats(); i++)
		{
			vstFormat = _audioPluginFormatManager.getFormat(i);
			if (vstFormat->getName() == juce::String(L"VST"))
			{
				break;
			}
		}

		Check(vstFormat != nullptr);

		FileSearchPath fileSearchPath{};
		for (const juce::String& path : _audioPluginSearchPaths)
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

		juce::String pluginBeingScanned{};
		// loop over all files synchronously, this is a bad experience if the path is huge so don't ever do that
		while (scanner.scanNextFile(/*dontRescanIfAlreadyInList*/true, pluginBeingScanned)) {};

		// and that's it!
		return true;
	}

	int NowSoundGraph::PluginCount()
	{
		return _knownPluginList.getNumTypes();
	}

	void NowSoundGraph::PluginName(PluginId pluginId, LPWSTR wcharBuffer, int32_t bufferCapacity)
	{
		juce::PluginDescription* desc = _knownPluginList.getType(((int)pluginId) - 1);
		const juce::String& name = desc->name;

		wcsncpy_s(wcharBuffer, (size_t)bufferCapacity, name.getCharPointer(), name.length());
	}

	void NowSoundGraph::PluginProgramCount(PluginId pluginId)
	{

	}

	// Get the name of the specified plugin's program.  Note that IDs are 1-based.
	void NowSoundGraph::PluginProgramName(PluginId pluginId, ProgramId programId, LPWSTR wcharBuffer, int32_t bufferCapacity)
	{

	}

	// static externally reachable shutdown method
	void NowSoundGraph::ShutdownInstance()
	{
		// SHUT. DOWN. EVERYTHING
		s_instance->Shutdown();

		// and then destruct the singleton
		s_instance = nullptr;

		// and destruct the clock
		// TODO: make the clock a component of the graph
		// (the clock has utility as a lower-level NowSoundLibShared-specific point of access to the sound data,
		// but having two singletons already led to bugs and is generally a bad smell)
		Clock::Shutdown();
	}

	// instance shutdown method for instance internal state
	void NowSoundGraph::Shutdown()
	{
		_audioDeviceManager.removeAllChangeListeners();
		_audioDeviceManager.closeAudioDevice();
		_audioDeviceManager.removeAudioCallback(&_audioProcessorPlayer);
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
		Check(JuceGraph().addConnection({ { newNode->nodeID, 0 }, { _audioOutputMixNodePtr->nodeID, 0 } }));
		Check(JuceGraph().addConnection({ { newNode->nodeID, 1 }, { _audioOutputMixNodePtr->nodeID, 1 } }));

		{
			std::wstringstream wstr{};
			wstr << L"NowSoundGraph::AddNodeToJuceGraph(" << newNode->nodeID.uid << L", " << inputChannel << ")";
			NowSoundGraph::Instance()->Log(wstr.str());
		}
	}

	void NowSoundGraph::LogConnections()
	{
		int maxConnNodeId = 0;
		for (auto conn : JuceGraph().getConnections())
		{
			std::wstringstream wstr;
			wstr << L"Connection: source " << conn.source.nodeID.uid << L"/" << conn.source.channelIndex
				<< ", destination " << conn.destination.nodeID.uid << L"/" << conn.destination.channelIndex;
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
		for (int i = 1; i <= maxConnNodeId; i++)
		{
			LogNode((juce::AudioProcessorGraph::NodeID)i);
		}
	}

	void NowSoundGraph::LogNode(juce::AudioProcessorGraph::NodeID nodeId)
	{
		const juce::AudioProcessorGraph::Node* nodePtr = JuceGraph().getNodeForId(nodeId);
		std::wstringstream wstr;
		wstr << L"Node #" << nodePtr->nodeID.uid << L": "
			<< nodePtr->getProcessor()->getName()
			<< L": bypassed " << nodePtr->isBypassed()
			<< L", suspended " << nodePtr->getProcessor()->isSuspended()
			<< L", latencySamples " << nodePtr->getProcessor()->getLatencySamples()
			<< L", sampleRate " << nodePtr->getProcessor()->getSampleRate()
			<< L", totalNumInputChannels " << nodePtr->getProcessor()->getTotalNumInputChannels()
			<< L", totalNumOutputChannels " << nodePtr->getProcessor()->getTotalNumOutputChannels()
			<< L", isUsingDoublePrecision " << nodePtr->getProcessor()->isUsingDoublePrecision();
		Log(wstr.str());
	}

	void NowSoundGraph::AsyncUpdate()
	{
		// This would seem to be unnecessary, but we need to ensure we do not lose a call to this method
		// in the event that the message tick thread is calling WasAsyncUpdate() concurrently with this,
		// resulting in the update here being lost (because WasAsyncUpdate() resets this flag).
		std::lock_guard<std::mutex> guard(_asyncUpdateMutex);
		_asyncUpdate = true;
	}

	bool NowSoundGraph::WasAsyncUpdate()
	{
		// This would seem to be unnecessary, but we need to ensure we do not lose a call to this method
		// in the event that the message tick thread is calling WasAsyncUpdate() concurrently with this,
		// resulting in the update here being lost (because WasAsyncUpdate() resets this flag).
		std::lock_guard<std::mutex> guard(_asyncUpdateMutex);
		bool result = _asyncUpdate;
		_asyncUpdate = false;
		return result;
	}

	void NowSoundGraph::MessageTick()
	{
		if (WasAsyncUpdate())
		{
			// call the JUCE graph's handleAsyncUpdate() method directly.
			_audioProcessorGraph.handleAsyncUpdate();
		}
	}
}
