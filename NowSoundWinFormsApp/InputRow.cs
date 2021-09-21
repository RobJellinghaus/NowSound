// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using NowSoundLib;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
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

        private readonly ComboBox _panCombo;

        private readonly ComboBox _effectCombo;

        /// <summary>
        /// The ID of the input monitored by this row.
        /// </summary>
        private readonly AudioInputId _audioInputId;

        /// <summary>
        /// FFT buffer.
        /// </summary>
        private readonly float[] _fftBuffer;

        /// <summary>
        /// Output buffer for the text-rendered FFT.
        /// </summary>
        private readonly StringBuilder _builder;

        /// <summary>
        /// The list of programs (e.g. sound effects we can apply).
        /// </summary>
        private readonly List<(string, PluginId, ProgramId)> _programs;

        /// <summary>
        /// The list of program instances (e.g. active sound effects on this input); each value here is a 1-based index into _programs.
        /// </summary>
        private readonly List<int> _programInstances;

        public InputRow(AudioInputId audioInputId, FlowLayoutPanel parent, List<(string, PluginId, ProgramId)> programs)
        {
            _audioInputId = audioInputId;
            _fftBuffer = new float[MagicConstants.OutputBinCount];
            _builder = new StringBuilder(new string('0', MagicConstants.OutputBinCount));
            _programs = programs;
            _programInstances = new List<int>();

            _label = new Label
            {
                Text = $"Input #{audioInputId}",
                MinimumSize = new Size(600, 20),
                MaximumSize = new Size(600, 20),
                TextAlign = ContentAlignment.BottomLeft,
                AutoSize = true
            };

            _panCombo = new ComboBox
            {
                MinimumSize = new Size(100, 20),
                MaximumSize = new Size(100, 20)
            };
            _panCombo.Items.AddRange(new[] { "Pan Left", "Pan Center", "Pan Right" });
            _panCombo.SelectedIndexChanged += PanComboSelectedIndexChanged;

            _effectCombo = new ComboBox
            {
                MinimumSize = new Size(100, 20),
                MaximumSize = new Size(100, 20)
            };
            _effectCombo.Items.AddRange(programs.Select(item => item.Item1).Cast<object>().ToArray());
            _effectCombo.SelectedIndexChanged += EffectComboSelectedIndexChanged;

            _trackRowPanel = new FlowLayoutPanel
            {
                AutoSize = true,
                FlowDirection = FlowDirection.LeftToRight
            };
            _trackRowPanel.Controls.Add(_label);
            _trackRowPanel.Controls.Add(_panCombo);
            _trackRowPanel.Controls.Add(_effectCombo);

            parent.Controls.Add(_trackRowPanel);
        }

        private void PanComboSelectedIndexChanged(object sender, EventArgs e)
        {
            int index = _panCombo.SelectedIndex;
            if (index >= 0)
            {
                float pan = (float)index / 2;
                NowSoundGraphAPI.SetInputPan(_audioInputId, pan);
            }
        }

        private void EffectComboSelectedIndexChanged(object sender, EventArgs e)
        {
            int index = _effectCombo.SelectedIndex;
            if (index >= 0)
            {
                // a program was picked.  Apply it
                NowSoundGraphAPI.AddInputPluginInstance(_audioInputId, _programs[index].Item2, _programs[index].Item3, 100);

                // and add a new label for it 
                _trackRowPanel.Controls.Add(new Label
                {
                    Text = _programs[index].Item1,
                    AutoSize = true,
                    MinimumSize = new Size(50, 20),
                    MaximumSize = new Size(50, 20),
                    TextAlign = ContentAlignment.BottomLeft
                });
                Button deleteButton = new Button
                {
                    Text = "X",
                    MinimumSize = new Size(20, 20),
                    MaximumSize = new Size(20, 20)
                };
                deleteButton.Click += DeleteButton_Click;
                _trackRowPanel.Controls.Add(deleteButton);

                // and spam
                NowSoundGraphAPI.LogConnections();
            }
        }

        private void DeleteButton_Click(object sender, EventArgs e)
        {
            // which button is this in the control list?
            int buttonIndex = 0;
            foreach (Control c in _trackRowPanel.Controls)
            {
                if (c is Button)
                {
                    buttonIndex++;
                }

                if (c == sender)
                {
                    // this is the button that was clicked.
                    // therefore buttonIndex is the 1-based index of this button.
                    // therefore buttonIndex equals the pluginInstanceIndex of the plugin we want to delete.
                    NowSoundGraphAPI.DeleteInputPluginInstance(_audioInputId, (PluginInstanceIndex)buttonIndex);

                    int index = _trackRowPanel.Controls.IndexOf(c);
                    // remove the label prior to the button
                    _trackRowPanel.Controls.RemoveAt(index - 1);
                    // and remove at index - 1 again, thereby removing the button itself
                    _trackRowPanel.Controls.RemoveAt(index - 1);

                    // and spam
                    NowSoundGraphAPI.LogConnections();

                    // and we're done
                    break;
                }
            }
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
