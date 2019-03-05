// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using NowSoundLib;
using System.Drawing;
using System.Windows.Forms;

namespace NowSoundWinFormsApp
{
    class TrackRow
    {
        /// <summary>
        /// The panel containing all controls for this track.
        /// </summary>
        private FlowLayoutPanel _trackRowPanel;

        /// <summary>
        /// Stop / delete button
        /// </summary>
        private Button _controlButton;

        /// <summary>
        /// Mute / unmute button
        /// </summary>
        private readonly Button _muteButton;

        /// <summary>
        /// Text label
        /// </summary>
        private readonly Label _label;

        /// <summary>
        /// The ID of the track controlled by this row.
        /// </summary>
        private readonly TrackId _trackId;

        public TrackRow(TrackId trackId, FlowLayoutPanel parent)
        {
            _trackId = trackId;

            _controlButton = new Button
            {
                Text = "Stop Recording",
                AutoSize = true
            };
            _controlButton.Click += ControlButton_Click;

            _muteButton = new Button
            {
                Text = "Mute",
                AutoSize = true,
                Enabled = false
            };

            _label = new Label
            {
                Text = $"Track #{trackId}",
                MinimumSize = new Size(400, 20),
                MaximumSize = new Size(400, 20),
                TextAlign = ContentAlignment.BottomLeft,
                AutoSize = true
            };

            _trackRowPanel = new FlowLayoutPanel
            {
                AutoSize = true
            };
            _trackRowPanel.Controls.Add(_controlButton);
            _trackRowPanel.Controls.Add(_muteButton);
            _trackRowPanel.Controls.Add(_label);

            parent.Controls.Add(_trackRowPanel);
        }

        private void ControlButton_Click(object sender, System.EventArgs e)
        {
            NowSoundTrackAPI.FinishRecording(_trackId);
        }

        public void Update()
        {
            TrackInfo trackInfo = NowSoundTrackAPI.Info(_trackId);

            _label.Text = $"Track {_trackId}: start {trackInfo.StartTimeInBeats}, duration {trackInfo.DurationInBeats}, current {trackInfo.LocalClockBeat}";

            /*
            << L" | Volume: " << trackInfo.Volume
            << L" | Last sample time: " << trackInfo.LastSampleTime
            << L" | Frequencies: " << _frequencyOutputString
            */
        }
    }
}
