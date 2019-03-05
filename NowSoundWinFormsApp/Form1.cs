// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using NowSoundLib;
using System.Windows.Forms;

namespace NowSoundWinFormsApp
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        private static int _nextTrackId = 0;

        private void _newTrackButton_Click(object sender, System.EventArgs e)
        {
            // we haven't started recording yet; time to do so!
            TrackId trackId = NowSoundGraphAPI.CreateRecordingTrackAsync(AudioInputId.AudioInput1);

            new TrackRow((TrackId)(++_nextTrackId), _tracksPanel);
        }
    }
}
