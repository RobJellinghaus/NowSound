﻿// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using NowSoundLib;
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

        public TrackRow(TrackId trackId, FlowLayoutPanel parent)
        {
            _trackId = trackId;
            _fftBuffer = new float[MagicConstants.OutputBinCount];
            _builder = new StringBuilder(new string('0', MagicConstants.OutputBinCount));

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
            NowSoundSignalInfo signalInfo = NowSoundTrackAPI.SignalInfo(_trackId);

            NowSoundTrackAPI.GetFrequencies(_trackId, _fftBuffer, _fftBuffer.Length);
            RenderFrequencyBuffer(_fftBuffer, _builder);

            _label.Text = $"Track {_trackId}: start {trackInfo.StartTimeInBeats}, duration {trackInfo.DurationInBeats}, current {trackInfo.LocalClockBeat}, "
                + $"maxsignal {signalInfo.Max:F4}, avgsignal {signalInfo.Avg:F4}, fft {_builder.ToString()}";

            if (trackInfo.IsTrackLooping)
            {
                _controlButton.Text = "Looping";
                _controlButton.Enabled = false;
            }
        }

        static void RenderFrequencyBuffer(float[] fftBuffer, StringBuilder builder)
        {
            // _frequencyBuffer is presumed to have been updated
            // first, get min/max of values
            double max = 0;
            for (int i = 0; i < MagicConstants.OutputBinCount; i++)
            {
                double value = fftBuffer[i];
                // drop out super tiny values -- experimentally values less than 1 are uninteresting.
                // This will make silence not have huge variance from tiny max values.
                if (value < 1)
                {
                    continue;
                }

                max = value > max ? value : max;
            }

            // scale to the max, dude
            if (max == 0)
            {
                builder.Clear();
                builder.Insert(0, "0", MagicConstants.OutputBinCount);
            }
            else
            {
                for (int i = 0; i < MagicConstants.OutputBinCount; i++)
                {
                    double scaledValue = fftBuffer[i] / max;
                    char digit = (char)((int)'0' + (int)(scaledValue * 9));
                    builder[i] = digit;
                }
            }
        }
    }
}
