// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include <future>
#include <vector>

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "Histogram.h"
#include "NowSoundLibTypes.h"
#include "rosetta_fft.h"
#include "SliceStream.h"

#include "JuceHeader.h"

namespace NowSound
{
	class BaseAudioProcessor;
	class SpatialAudioProcessor;
    class NowSoundInputAudioProcessor;
	class NowSoundTrackAudioProcessor;

	class PluginProgram
	{
	private:
		MemoryBlock _state;
		String _name;

	public:
		const MemoryBlock& State() { return _state; }
		const String& Name() { return _name; }
		
		PluginProgram(const MemoryBlock& memoryBlock, const String& name) : _state{ memoryBlock }, _name{ name }
		{}
	};

    // A single graph implementing the NowSoundGraphAPI operations.
    class NowSoundGraph
    {
    public: // API methods called by the NowSoundGraphAPI P/Invoke bridge methods

        // Initialize the audio graph subsystem.
        // Graph must be Uninitialized.  On completion, graph becomes Initialized.
        void Initialize(
			int outputBinCount,
			float centralFrequency,
			int octaveDivisions,
			int centralBinIndex,
			int fftSize);

        // Get the current state of the audio graph; intended to be efficiently pollable by the client.
        // This is one of the only two methods that may be called in any state whatoever.
        // All other methods declare which state the graph must be in to call the method, and the state
        // the method transitions the graph to once the asynchronous action is complete.
        // TODO: consider having some separate mutual exclusion to prevent multiple concurrent methods
        // from firing (don't want the graph to, e.g., get started twice in a race).
        NowSoundGraphState State() const;

		// Get the graph info for the created graph.
		// Graph must be at least Initialized.
		NowSoundGraphInfo Info();

		// Information about the raw mono input signal.
		// Graph must be at least Initialized.
		NowSoundSignalInfo RawInputSignalInfo(AudioInputId audioInputId);

		// Information about a post-effects input signal.
		// Graph must be at least Initialized.
		NowSoundSignalInfo InputSignalInfo(AudioInputId audioInputId);

		// Information about the final mixed output signal.
		// Graph must be at least Initialized.
		NowSoundSignalInfo OutputSignalInfo();

		// Get the ID of the input device with the given index (from 0 to Info().InputDeviceCount-1).
		// JUCETODO: void InputDeviceId(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity);

		// Get the name of the input device with the given index (from 0 to Info().InputDeviceCount-1).
		// JUCETODO: void InputDeviceName(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity);

		// Initialize given input device.  One mono input will be created per channel of the device.
		// This must be called only in Initialized state (for now; could relax this later perhaps).
		// JUCETODO: void InitializeDeviceInputs(int deviceIndex);

		// Info about the current graph time.
		// Graph must be Created or Running.
		NowSoundTimeInfo TimeInfo();

		// The current log info.
		NowSoundLogInfo LogInfo();

		// Get a log message.
		void GetLogMessage(int32_t logMessageIndex, LPWSTR buffer, int32_t bufferCapacity);

		// Drop all messages up to (and including) the given log message index.
		void DropLogMessages(int32_t messageCountToDrop);

        // Create a new track and begin recording.
        // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
		TrackId CreateRecordingTrackAsync(AudioInputId inputIndex);

		// Call this regularly from the message thread.
		// This is a gross hack to work around the fact that running JUCE as a native plugin (not a Unity plugin)
		// under Unity breaks JUCE's built-in async message pumping, resulting in async graph structure changes not getting
		// properly handled on the message thread.
		// The JucePlugin_Build_Unity preprocessor flag affects this behavior, but I am not going to mess with that just yet.
		void MessageTick();

	public: // Plugin support

		// Plugin searching requires setting paths to search.
		// TODO: make this use the idiom for passing in strings rather than StringBuilders.
		void AddPluginSearchPath(LPWSTR wcharBuffer, int32_t bufferCapacity);

		// After setting one or more search paths, actually search.
		// TODO: make this asynchronous.
		// Returns true if no errors in searching, or false if there were errors (printed to debug log, hopefully).
		bool SearchPluginsSynchronously();

		// How many plugins?
		int PluginCount();

		// Get the name of the Nth plugin. Note that IDs are 1-based.
		void PluginName(PluginId pluginId, LPWSTR wcharBuffer, int32_t bufferCapacity);

		// Load all programs in this directory and associate with the given PluginId.
		bool LoadPluginPrograms(PluginId pluginId, LPWSTR pathnameBuffer);

		// Get the number of programs for the given plugin.
		int PluginProgramCount(PluginId pluginId);

		// Get the name of the specified plugin's program.  Note that IDs are 1-based.
		void PluginProgramName(PluginId pluginId, ProgramId programId, LPWSTR wcharBuffer, int32_t bufferCapacity);

	private: // Constructor and internal implementations

        // construct a graph, but do not yet initialize it
        NowSoundGraph();

        // Check that the expected state is the current state, and that no current state change is happening;
        // then mark that a state change is now happening.
        void PrepareToChangeState(NowSoundGraphState expectedState);

        // Check that a state change is happening, then switch the state to newState and mark the state change
        // as no longer happening.
        void ChangeState(NowSoundGraphState newState);

        // Set minimum buffer size in the device manager.
        void setBufferSize();

		// Record that an async update happened.
		void AsyncUpdate();

		// Did an async update happen since the last call to this method?
		bool WasAsyncUpdate();

    private: // instance variables

        // The singleton (for now) graph; created by Initialize(), destroyed by Shutdown().
        static ::std::unique_ptr<NowSoundGraph> s_instance;

		// Fixed capacity for log messages (between calls to DropLogMessagesUpTo()).
		const int32_t s_logMessageCapacity = 10000;

		// Log messages.
		// Note that this vector is always allocated to a fixed capacity so we don't get vector resizes while appending.
		std::vector<std::wstring> _logMessages;

		// The mutex used when updating log state variables.
		std::mutex _logMutex;

		// The AudioDeviceManager held by this Graph.
		// This is conceptually a singleton (just as the NowSoundGraph is), but we scope it within this type.
		juce::AudioDeviceManager _audioDeviceManager;

		// Callback object which couples the device manager to the audio processor graph.
		juce::AudioProcessorPlayer _audioProcessorPlayer;

		// The audio processor graph.
		juce::AudioProcessorGraph _audioProcessorGraph;

        // Ptr to the input node.
        juce::AudioProcessorGraph::Node::Ptr _audioInputNodePtr;

        // Ptr to the final mix node.
        juce::AudioProcessorGraph::Node::Ptr _audioOutputMixNodePtr;

        // Ptr to the actual output node.
        juce::AudioProcessorGraph::Node::Ptr _audioOutputNodePtr;

        // Information about the output signal.
        // Atomically updated using the associated mutex.
        NowSoundSignalInfo _outputSignalInfo;

        // Mutex guarding _outputSignalInfo, which may be polled from the UI thread while being
        // updated on the audio thread.
        std::mutex _outputSignalMutex;

        // Is this graph changing state? (Prevent re-entrant state changing methods.)
        bool _changingState;

        // The state of this graph.
        NowSoundGraphState _audioGraphState;

        // First, an allocator for 128-second 48Khz stereo float sample buffers.
        std::unique_ptr<BufferAllocator<float>> _audioAllocator;

        // The next TrackId to be allocated.
        TrackId _nextTrackId;

		// The next AudioInputId to be allocated.
		AudioInputId _nextAudioInputId;

		// The vector of frequency bins.
		::std::vector<RosettaFFT::FrequencyBinBounds> _fftBinBounds;

		// The FFT size.
		int _fftSize;

		// The audio inputs we have; currently unchanging after graph creation.
		// Note that this vector does not own the processors; the JUCE graph does.
		// TODO: vaguely consider supporting dynamically added/removed inputs.
		std::vector<NowSoundInputAudioProcessor*> _audioInputs;

        // Mutex for the state of the graph.
        // The combination of _audioGraphState and _changingState must be updated atomically, or hazards are possible.
        std::mutex _stateMutex;

		int _logThrottlingCounter;

		// The collection of all tracks.
		// Note that this vector does not own the processors; the JUCE graph does.
		std::map<TrackId, NowSoundTrackAudioProcessor*> _tracks;

		// True if there was an async update.
		bool _asyncUpdate;

		// Mutex for changing the state of _asyncUpdate.
		// This is to avoid a potential race between testing _asyncUpdate to find it true,
		// and then resetting it to false concurrently with an audio thread setting it to true again.
		std::mutex _asyncUpdateMutex;

		// Vector of search paths for plugin searching.
		std::vector<juce::String> _audioPluginSearchPaths;

		// List of known plugins.
		juce::KnownPluginList _knownPluginList;

		// Manager of known plugin formats.
		juce::AudioPluginFormatManager _audioPluginFormatManager;

		// Place to keep an exception message if we need to throw one.
		std::string _exceptionMessage;

		// Vector, indexed by plugin ID (minus 1), of vectors of PluginPrograms.
		// Only plugins that have had LoadPluginPrograms called for them will be in this list.
		std::vector<std::vector<PluginProgram>> _loadedPluginPrograms;

	public:
		// non-exported methods for "internal" use
		void AddTrack(TrackId id, NowSoundTrackAudioProcessor* track);

		// Accessor for track by ID.
		NowSoundTrackAudioProcessor* Track(TrackId id);

		// Check to see if this track ID exists. TODO: DELETE THIS; JUST FOR USE WHEN RACE HUNTING.
		// Should be synchronously the case that track IDs are never queried before they are actually defined!
		bool TrackIsDefined(TrackId id);

		void DeleteTrack(TrackId id);

	public: // Implementation methods used from elsewhere in the library

        // The static instance of the graph.  We may eventually have multiple.
        static NowSoundGraph* Instance();

		// Create the singleton graph instance and initialize it.
		static void InitializeInstance(
			int outputBinCount,
			float centralFrequency,
			int octaveDivisions,
			int centralBinIndex,
			int fftSize);


        // These methods are for "internal" use only (since they not dllexported and are not using exportable types).

		// Record this log message.
		// These messages can be queried via the external NowSoundGraphAPI, for scenarios when native debugging is
		// inaccessible (such as VS2019 debugging Unity with the Mono runtime).
		void Log(const std::wstring& str);
		
		// Audio allocator has static lifetime currently, but we give borrowed pointers rather than just statically
        // referencing it everywhere, because all this mutable static state continues to be concerning.
        BufferAllocator<float>* AudioAllocator() const;

		// Create a NowSoundInputAudioProcessor for the specified channel.
		void CreateNowSoundInputForChannel(int channel);

		// Access the vector of frequency bins, when generating frequency histograms.
		const std::vector<RosettaFFT::FrequencyBinBounds>* BinBounds() const;

		// Access to the FFT size.
		int FftSize() const;

        // Access to the audio graph for node instantiation.
        juce::AudioProcessorGraph& JuceGraph();

		// Get a reference-counted reference on this BaseAudioProcessor.
		// The processor must have had its node ID set.
		juce::AudioProcessorGraph::Node::Ptr GetNodePtr(BaseAudioProcessor* processor);

        // Get a reference on one of the NowSoundInputs.
        NowSoundInputAudioProcessor* Input(AudioInputId audioInputId);

        // Add the connections of a SpatialAudioProcessor node.
        // This entails connecting the given inputChannel to newSpatialNode's channel 0,
        // and connecting all newSpatialNode's OutputProcessor()'s output channels to the graph's output channels.
        // (Note that this does not modify this NowSoundGraph's collection of Inputs or of Tracks; 
        // this just makes the JUCE graph connections.)
        // For now this always sets up one input connection and two output connections.
        void AddNodeToJuceGraph(SpatialAudioProcessor* newSpatialNode, int inputChannel);

		// Construct a stereo AudioProcessor for the given plugin and program.
		// The returned reference is unowned and raw; this needs to be added to the JUCE AudioProcessorGraph immediately.
		AudioProcessor* CreatePluginProcessor(PluginId pluginId, ProgramId programId);

		// Log the current connections in the graph
		void LogConnections();

		// Log a single node in all detail.
		void LogNode(juce::AudioProcessorGraph::NodeID nodeId);

		bool CheckLogThrottle();

		// Actually shut down the audio processing.
		void Shutdown();

		// Shut down the singleton graph instance and release it.
		static void ShutdownInstance();
	};
}
