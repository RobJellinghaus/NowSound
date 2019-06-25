// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using NowSoundLib;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;

namespace NowSoundWinFormsApp
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();

            timer1.Start();
        }

        private List<TrackRow> _trackRows = new List<TrackRow>();

        private StringBuilder _logBuilder = new StringBuilder(1024);

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

            for (int i = 0; i < _trackRows.Count; i++)
            {
                _trackRows[i].Update();
            }

            WriteAllLogMessagesToDebugConsole();
        }
    }
}
