// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using NowSoundLib;
using System.Collections.Generic;
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

        private static int _nextTrackId = 0;

        private List<TrackRow> _trackRows = new List<TrackRow>();

        private void _newTrackButton_Click(object sender, System.EventArgs e)
        {
            // we haven't started recording yet; time to do so!
            TrackId trackId = NowSoundGraphAPI.CreateRecordingTrackAsync(AudioInputId.AudioInput1);

            _trackRows.Add(new TrackRow((TrackId)(++_nextTrackId), _tracksPanel));
        }

        private void timer1_Tick(object sender, System.EventArgs e)
        {
            NowSoundGraphInfo info = NowSoundGraphAPI.Info();
            TimeInfo timeInfo = NowSoundGraphAPI.TimeInfo();
            infoLabel.Text = $"Sample rate {info.SampleRate}, buffer size {info.SamplesPerQuantum}, sample time {timeInfo.TimeInSamples}, exact beat {timeInfo.ExactBeat}";

            for (int i = 0; i < _trackRows.Count; i++)
            {
                _trackRows[i].Update();
            }
        }
    }
}
