// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using NowSoundLib;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
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
            StringBuilder buffer = new StringBuilder(recordingFile);
            Debug.WriteLine($"Form1.StartRecording(): Starting recording to file {recordingFile}");

            _isRecordingToFile = true;
            NowSoundGraphAPI.StartRecording(buffer);
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
