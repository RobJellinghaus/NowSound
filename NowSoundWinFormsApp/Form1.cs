// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using NowSoundLib;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace NowSoundWinFormsApp
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        private List<InputRow> _inputRows = new List<InputRow>();

        private List<TrackRow> _trackRows = new List<TrackRow>();

        private StringBuilder _logBuilder = new StringBuilder(1024);

        /// <summary>
        /// Are we currently recording output audio into a file?
        /// </summary>
        private bool _isRecordingToFile = false;

        public bool IsRecordingToFile => _isRecordingToFile;

        private void _newTrackButton_Click(object sender, System.EventArgs e)
        {
            // we haven't started recording yet; time to do so!
            TrackId trackId = NowSoundGraphAPI.CreateRecordingTrackAsync(AudioInputId.AudioInput1);

            _trackRows.Add(new TrackRow(trackId, _tracksPanel, this.RemoveTrack));
        }

        private void RemoveTrack(TrackId trackId)
        {
            for (int i = 0; i < _trackRows.Count; i++)
            {
                if (_trackRows[i].TrackId == trackId)
                {
                    _trackRows.RemoveAt(i);
                    break;
                }
            }
        }

        public void WriteAllLogMessagesToDebugConsole()
        {
            NowSoundLogInfo logInfo = NowSoundGraphAPI.LogInfo();
            if (logInfo.LogMessageCount > 0)
            {
                Console.WriteLine("Log message count: " + logInfo.LogMessageCount);
                for (int i = 0; i < logInfo.LogMessageCount; i++)
                {
                    _logBuilder.Clear();
                    NowSoundGraphAPI.GetLogMessage(i, _logBuilder);
                    Console.WriteLine($"NOWSOUNDLIB LOG: {_logBuilder}");
                }
                NowSoundGraphAPI.DropLogMessages(logInfo.LogMessageCount);
            }
        }

        protected override async void OnLoad(EventArgs e)
        {
            base.OnLoad(e);

            // let's see when this gets called
            Console.WriteLine("OnLoad");

            // These constant values obtained by experiment relative to my personal baritone :-P
            // and let's P/Invoke up in here!
            NowSoundGraphAPI.InitializeInstance(
                MagicConstants.OutputBinCount,
                MagicConstants.CentralFrequency,
                MagicConstants.OctaveDivisions,
                MagicConstants.CentralFrequencyBin,
                MagicConstants.FftBinSize,
                MagicConstants.PreRecordingDuration);

            // No longer necessary really since JUCE initialization is synchronous.
            // JUCETODO: clean this up eventually.
            bool reachedState = await AwaitAudioGraphState(NowSoundGraphState.GraphRunning, timeoutMsec: 10);

            Console.WriteLine($"Pseudo-awaited; reachedState {reachedState}");

            // Now let's scan!
            NowSoundGraphAPI.AddPluginSearchPath(@"C:\Program Files\Steinberg\VSTPlugins");
            NowSoundGraphAPI.AddPluginSearchPath(@"C:\Program Files\VSTPlugins");

            bool searched = NowSoundGraphAPI.SearchPluginsSynchronously();
            Contract.Assert(searched);

            int pluginCount = NowSoundGraphAPI.PluginCount();
            StringBuilder buffer = new StringBuilder(100);

            string presetDirectory = @"C:\git\holofunk2\presets";

            List<(string, PluginId, ProgramId)> programs = new List<(string, PluginId, ProgramId)>();
            for (PluginId pluginIndex = (PluginId)1; pluginIndex <= (PluginId)pluginCount; pluginIndex++)
            {
                NowSoundGraphAPI.PluginName((PluginId)pluginIndex, buffer);
                string pluginName = buffer.ToString();
                Console.WriteLine($"Plugin #{pluginIndex}: '{pluginName}'");

                string pluginPresetPath = Path.Combine(presetDirectory, pluginName);
                if (Directory.Exists(pluginPresetPath))
                {
                    NowSoundGraphAPI.LoadPluginPrograms((PluginId)pluginIndex, pluginPresetPath);

                    int programCount = NowSoundGraphAPI.PluginProgramCount((PluginId)pluginIndex);
                    for (ProgramId programIndex = (ProgramId)1; programIndex <= (ProgramId)programCount; programIndex++)
                    {
                        NowSoundGraphAPI.PluginProgramName((PluginId)pluginIndex, (ProgramId)programIndex, buffer);
                        string programName = buffer.ToString();
                        programs.Add(($"{pluginName[0]}: {programName}", pluginIndex, programIndex));
                        Console.WriteLine($"    Program #{programIndex}: '{programName}'");
                    }
                }
            }

            _inputRows.Add(new InputRow(AudioInputId.AudioInput1, flowLayoutPanel0, programs));
            _inputRows.Add(new InputRow(AudioInputId.AudioInput2, flowLayoutPanel1, programs));

            // Now we can begin UI updating.
            timer1.Start();
        }

        private async Task<bool> AwaitAudioGraphState(NowSoundGraphState desiredState, int timeoutMsec)
        {
            DateTime startTime = DateTime.Now;

            // TODO: do something
            while (true)
            {
                NowSoundGraphState currentState = NowSoundGraphAPI.State();
                if (currentState == desiredState)
                {
                    return true;
                }
                if (((DateTime.Now.Ticks - startTime.Ticks) / 10000) > timeoutMsec)
                {
                    return false;
                }

                await Task.Delay(1);
            }
        }

        private void timer1_Tick(object sender, System.EventArgs e)
        {
            NowSoundGraphInfo info = NowSoundGraphAPI.Info();
            TimeInfo timeInfo = NowSoundGraphAPI.TimeInfo();
            NowSoundSignalInfo outputSignalInfo = NowSoundGraphAPI.OutputSignalInfo();
            infoLabel.Text = $"Sample rate {info.SampleRate}, buffer size {info.SamplesPerQuantum}, sample time {timeInfo.TimeInSamples}, exact beat {timeInfo.ExactBeat}, "
                + $"maxsignal {outputSignalInfo.Max:F4}, avgsignal {outputSignalInfo.Avg:F4}";

            for (int i = 0; i < _inputRows.Count; i++)
            {
                _inputRows[i].Update();
            }
            for (int i = 0; i < _trackRows.Count; i++)
            {
                _trackRows[i].Update();
            }

            WriteAllLogMessagesToDebugConsole();
        }

        public void StartRecording()
        {
            Contract.Requires(!IsRecordingToFile);

            string myDocumentsPath = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
            string recordingsPath = Path.Combine(myDocumentsPath, "HolofunkRecordings");
            Directory.CreateDirectory(recordingsPath);
            Debug.WriteLine($"Form1.StartRecording(): Created directory {recordingsPath}");

            DateTime now = DateTime.Now;
            string recordingFile = Path.Combine(recordingsPath, now.ToString("yyyyMMdd_HHmmss.wav"));
            Debug.WriteLine($"Form1.StartRecording(): Starting recording to file {recordingFile}");

            _isRecordingToFile = true;
            NowSoundGraphAPI.StartRecording(recordingFile);
        }

        public void StopRecording()
        {
            Contract.Requires(IsRecordingToFile);

            _isRecordingToFile = false;
            NowSoundGraphAPI.StopRecording();
        }

        private void RecordToFileButton_Click(object sender, EventArgs e)
        {
            if (IsRecordingToFile)
            {
                StopRecording();
                recordToFileButton.Text = "Record audio to file";
            }
            else
            {
                StartRecording();
                recordToFileButton.Text = "Stop recording to file";
            }
        }
    }
}
