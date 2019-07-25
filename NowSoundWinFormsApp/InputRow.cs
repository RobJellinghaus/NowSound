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
    class InputRow
    {
        /// <summary>
        /// The panel containing all controls for this track.
        /// </summary>
        private FlowLayoutPanel _trackRowPanel;

        /// <summary>
        /// Text label
        /// </summary>
        private readonly Label _label;

        /// <summary>
        /// The ID of the input monitored by this row.
        /// </summary>
        private readonly AudioInputId _audioInputId;

        /// <summary>
        /// FFT buffer.
        /// </summary>
        private float[] _fftBuffer;

        /// <summary>
        /// Output buffer for the text-rendered FFT.
        /// </summary>
        private StringBuilder _builder;

        public InputRow(AudioInputId audioInputId, FlowLayoutPanel parent)
        {
            _audioInputId = audioInputId;
            _fftBuffer = new float[MagicConstants.OutputBinCount];
            _builder = new StringBuilder(new string('0', MagicConstants.OutputBinCount));

            _label = new Label
            {
                Text = $"Input #{audioInputId}",
                MinimumSize = new Size(600, 20),
                MaximumSize = new Size(600, 20),
                TextAlign = ContentAlignment.BottomLeft,
                AutoSize = true
            };

            _trackRowPanel = new FlowLayoutPanel
            {
                AutoSize = true
            };
            _trackRowPanel.Controls.Add(_label);

            parent.Controls.Add(_trackRowPanel);
        }

        public void Update()
        {
            NowSoundSignalInfo rawSignalInfo = NowSoundGraphAPI.RawInputSignalInfo(_audioInputId);
            NowSoundSignalInfo signalInfo = NowSoundGraphAPI.InputSignalInfo(_audioInputId);

            NowSoundGraphAPI.GetInputFrequencies(_audioInputId, _fftBuffer, _fftBuffer.Length);
            Utilities.RenderFrequencyBuffer(_fftBuffer, _builder);

            _label.Text = $"Input {_audioInputId}: rawmax {rawSignalInfo.Max:F4}, rawavg {rawSignalInfo.Avg:F4}, max {signalInfo.Max:F4}, avg {signalInfo.Avg:F4}, fft {_builder.ToString()}";
        }
    }
}
