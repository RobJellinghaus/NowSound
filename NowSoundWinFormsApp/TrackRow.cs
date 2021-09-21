// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using NowSoundLib;
using System;
using System.Diagnostics;
using System.Drawing;
using System.Text;
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

        private readonly ComboBox _panCombo;

        /// <summary>
        /// Text label
        /// </summary>
        private readonly Label _label;

        /// <summary>
        /// The ID of the track controlled by this row.
        /// </summary>
        private readonly TrackId _trackId;

        /// <summary>
        /// FFT buffer.
        /// </summary>
        private float[] _fftBuffer;

        /// <summary>
        /// Output buffer for the text-rendered FFT.
        /// </summary>
        private StringBuilder _builder;

        /// <summary>
        /// An action that can remove this TrackRow.
        /// </summary>
        private Action<TrackId> _removeAction;

        internal TrackId TrackId => _trackId;

        public TrackRow(TrackId trackId, FlowLayoutPanel parent, Action<TrackId> removeAction)
        {
            _trackId = trackId;
            _fftBuffer = new float[MagicConstants.OutputBinCount];
            _builder = new StringBuilder(new string('0', MagicConstants.OutputBinCount));
            _removeAction = removeAction;

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
            _muteButton.Click += _muteButton_Click;

            _panCombo = new ComboBox
            {
                MinimumSize = new Size(100, 20),
                MaximumSize = new Size(100, 20)
            };
            _panCombo.Items.AddRange(new[] { "Pan Left", "Pan Center", "Pan Right" });
            _panCombo.SelectedIndexChanged += PanComboSelectedIndexChanged;

            _label = new Label

            {
                Text = $"Track #{trackId}",
                MinimumSize = new Size(600, 20),
                MaximumSize = new Size(600, 20),
                TextAlign = ContentAlignment.BottomLeft,
                AutoSize = true
            };

            _trackRowPanel = new FlowLayoutPanel
            {
                AutoSize = true
            };
            _trackRowPanel.Controls.Add(_controlButton);
            _trackRowPanel.Controls.Add(_muteButton);
            _trackRowPanel.Controls.Add(_panCombo);
            _trackRowPanel.Controls.Add(_label);

            parent.Controls.Add(_trackRowPanel);
        }

        private void _muteButton_Click(object sender, EventArgs e)
        {
            bool isMuted = NowSoundTrackAPI.IsMuted(_trackId);
            NowSoundTrackAPI.SetIsMuted(_trackId, !isMuted);
            _muteButton.Text = isMuted ? "Mute" : "Unmute";
        }

        private void PanComboSelectedIndexChanged(object sender, EventArgs e)
        {
            int index = _panCombo.SelectedIndex;
            if (index >= 0)
            {
                float pan = (float)index / 2;
                NowSoundTrackAPI.SetPan(_trackId, pan);
            }
        }

        private void ControlButton_Click(object sender, System.EventArgs e)
        {
            TrackInfo trackInfo = NowSoundTrackAPI.Info(_trackId);
            if (!trackInfo.IsTrackLooping)
            {
                NowSoundTrackAPI.FinishRecording(_trackId);
                _muteButton.Enabled = true;
            }
            else
            {
                _trackRowPanel.Parent.Controls.Remove(_trackRowPanel);
                _removeAction(_trackId);
                NowSoundGraphAPI.DeleteTrack(_trackId);
            }
        }

        public void Update()
        {
            TrackInfo trackInfo = NowSoundTrackAPI.Info(_trackId);
            NowSoundSignalInfo signalInfo = NowSoundTrackAPI.SignalInfo(_trackId);

            NowSoundTrackAPI.GetFrequencies(_trackId, _fftBuffer);
            Utilities.RenderFrequencyBuffer(_fftBuffer, _builder);

            _label.Text = $"Track {_trackId}: start {trackInfo.StartTimeInBeats}, duration {trackInfo.DurationInBeats}, current {trackInfo.LocalClockBeat}, "
                + $"maxsignal {signalInfo.Max:F4}, avgsignal {signalInfo.Avg:F4}, fft {_builder.ToString()}";

            if (trackInfo.IsTrackLooping)
            {
                _controlButton.Text = "Delete";
            }
            else
            {
                _controlButton.Text = "Finish";
            }
        }
    }
}
