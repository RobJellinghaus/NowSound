// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include <future>
#include <vector>

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "Clock.h"
#include "Histogram.h"
#include "NowSoundLibTypes.h"
#include "rosetta_fft.h"
#include "SliceStream.h"
#include "Tempo.h"

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

    /// <summary>
    /// Types of nodes in our graph.
    /// </summary>
    enum class NodeType
    {
        // zero value is undefined, as usual
        Undefined,
        // input node; has one input channel and two output channels
        Input,
        // recording node; has two input channels and two output channels
        Recording,
        // looping node; has zero input channels and two output channels
        Looping,
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
            int fftSize,
            float preRecordingDuration);

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

        // Set the tempo of the graph (really the tempo of currently recorded inputs).
        void SetTempo(float beatsPerMinute, int beatsPerMeasure);

        // The current log info.
        NowSoundLogInfo LogInfo();

        // Get a log message.
        void GetLogMessage(int32_t logMessageIndex, LPWSTR buffer, int32_t bufferCapacity);

        // Drop all messages up to (and including) the given log message index.
        void DropLogMessages(int32_t messageCountToDrop);

        // Log the current connections in the graph
        void LogConnections();

        // Create a new track and begin recording.
        // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
        TrackId CreateRecordingTrackAsync(AudioInputId inputIndex);

        // Create a copy of a track that has finished recording and started looping; the copy will be at the
        // exact same loop position.
        TrackId CopyLoopingTrack(TrackId trackToCopy);

        // Call this regularly from the message thread.
        // This is a gross hack to work around the fact that running JUCE as a native plugin (not a Unity plugin)
        // under Unity breaks JUCE's built-in async message pumping, resulting in async graph structure changes not getting
        // properly handled on the message thread.
        // The JucePlugin_Build_Unity preprocessor flag affects this behavior, but I am not going to mess with that just yet.
        void MessageTick();

        // Start recording to the given filename (WAV format); if already recording, this is ignored.
        void StartRecording(LPWSTR fileName, int32_t fileNameLength);

        // Stop recording and close the file; if not recording, this is ignored.
        void StopRecording();

        // Get the pan value of this input (0 = left; 0.5 = center; 1 = right)
        float InputPan(AudioInputId id);

        // Set the pan value of this input (0 = left; 0.5 = center; 1 = right)
        void InputPan(AudioInputId id, float pan);

    public: // Plugin support

        // Plugin searching requires setting paths to search.
        // TODO: make this use the idiom for passing in strings rather than StringBuilders.
        void AddPluginSearchPath(LPWSTR wcharBuffer, int32_t bufferCapacity);

        // After setting one or more search paths, actually search.
        // TODO: make this asynchronous.
        // Returns true if no errors in searching, or false if there were errors (printed to debug log, hopefully).
        bool SearchPluginsSynchronously();

        void ScanPluginFormat(juce::AudioPluginFormat* vstFormat);

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

        // Was the JUCE audio processor graph changed since the last call to this method?
        bool WasJuceGraphChanged();

        // Add the connections of a SpatialAudioProcessor node.
        // If this is an input node, this returns the input node ID so that input connections can be set up.
        // The number of channels defiend for the node will depend on the nodeType.
        AudioProcessorGraph::NodeID AddNodeToJuceGraph(SpatialAudioProcessor* newSpatialNode, NodeType nodeType);

    private: // instance variables

        // The singleton (for now) graph; created by Initialize(), destroyed by Shutdown().
        static ::std::unique_ptr<NowSoundGraph> s_instance;

        // Fixed capacity for log messages (between calls to DropLogMessagesUpTo()).
        const int32_t s_logMessageCapacity = 10000;

        // The singleton Clock. (This uses unique_ptr since it is owned but we want to be able to initialize it as null.)
        std::unique_ptr<NowSound::Clock> _clock;

        // The current Tempo.
        std::unique_ptr<NowSound::Tempo> _tempo;

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

        // The audio processor graph. Held via unique ptr so it can be dropped explicitly.
        std::unique_ptr<juce::AudioProcessorGraph> _audioProcessorGraph;

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

        // The amount of time to "pre-record" as latency compensation.
        ContinuousDuration<Second> _preRecordingDuration;

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

        // True if the JUCE graph was changed.
        bool _juceGraphChanged;

        // Mutex for changing the state of _juceGraphChanged.
        // This is to avoid a potential race between testing _juceGraphChanged to find it true,
        // and then resetting it to false concurrently with an audio thread setting it to true again.
        std::mutex _juceGraphChangedMutex;

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
        // Internal accessors and helpers.

        // The current graph-wide Tempo (for inputs)
        Tempo* Tempo() const;

        // The graph-wide Clock.
        Clock* Clock() const;

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
            int fftSize,
            float preRecordingDuration);

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

        // The amount of time to "pre-record" by, as latency compensation
        ContinuousDuration<Second> PreRecordingDuration() const;

        // Access to the audio graph for node instantiation.
        juce::AudioProcessorGraph& JuceGraph();

        // Get a reference-counted reference on this BaseAudioProcessor.
        // The processor must have had its node ID set.
        juce::AudioProcessorGraph::Node::Ptr GetNodePtr(BaseAudioProcessor* processor);

        // Get a reference on one of the NowSoundInputs.
        NowSoundInputAudioProcessor* Input(AudioInputId audioInputId);

        // Add the connections of a SpatialAudioProcessor node consuming and spatializing a single-channel input.
        // This sets up one input connection and two output connections.
        void AddInputNodeToJuceGraph(SpatialAudioProcessor* newSpatialNode, int inputChannel);

        // Add the connections of a SpatialAudioProcessor node consuming (not spatializing) a two-channel input.
        // This sets up two input connections and two output connections.
        void AddRecordingNodeToJuceGraph(SpatialAudioProcessor* newSpatialNode, AudioInputId audioInputId);

        // Construct a stereo AudioProcessor for the given plugin and program.
        // The returned reference is unowned and raw; this needs to be added to the JUCE AudioProcessorGraph immediately.
        AudioProcessor* CreatePluginProcessor(PluginId pluginId, ProgramId programId);

        // Log a single node in all detail.
        void LogNode(juce::AudioProcessorGraph::NodeID nodeId);

        bool CheckLogThrottle();

        // Record that an async update happened (e.g. a change to the JUCE audio processor graph, that needs to cause
        // the audio rendering graph to be recreated).
        // If we weren't running JUCE in such a hacky way under Unity, this wouldn't be needed.
        void JuceGraphChanged();

        // Actually shut down the audio processing.
        void Shutdown();

        // Shut down the singleton graph instance and release it.
        static void ShutdownInstance();
    };
}
