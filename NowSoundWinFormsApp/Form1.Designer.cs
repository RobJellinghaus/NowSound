// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using NowSoundLib;
using System;
using System.Diagnostics;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace NowSoundWinFormsApp
{
    partial class Form1
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            NowSoundGraphAPI.ShutdownInstance();
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        public void WriteAllLogMessagesToDebugConsole()
        {
            NowSoundLogInfo logInfo = NowSoundGraphAPI.LogInfo();
            if (logInfo.LogMessageCount > 0)
            {
                StringBuilder builder = new StringBuilder(256);
                for (int i = 0; i <= logInfo.LogMessageCount; i++)
                {
                    builder.Clear();
                    NowSoundGraphAPI.GetLogMessage(i, builder);
                    Debug.WriteLine(builder.ToString());
                }
                NowSoundGraphAPI.DropLogMessages(logInfo.LogMessageCount);
            }
        }

        protected override async void OnLoad(EventArgs e)
        {
            base.OnLoad(e);

            // let's see when this gets called
            Console.WriteLine("OnLoad");

            // and let's P/Invoke up in here!
            NowSoundGraphAPI.InitializeInstance();

            // and dump any output
            WriteAllLogMessagesToDebugConsole();

            // No longer necessary really since JUCE initialization is synchronous.
            // JUCETODO: clean this up eventually.
            bool reachedState = await AwaitAudioGraphState(NowSoundGraphState.GraphRunning, timeoutMsec: 10);

            Console.WriteLine($"Pseudo-awaited; reachedState {reachedState}");
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

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this._tracksPanel = new System.Windows.Forms.FlowLayoutPanel();
            this._newTrackButton = new System.Windows.Forms.Button();
            this.timer1 = new System.Windows.Forms.Timer(this.components);
            this.infoLabel = new System.Windows.Forms.Label();
            this._tracksPanel.SuspendLayout();
            this.SuspendLayout();
            // 
            // _tracksPanel
            // 
            this._tracksPanel.Controls.Add(this.infoLabel);
            this._tracksPanel.Controls.Add(this._newTrackButton);
            this._tracksPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._tracksPanel.FlowDirection = System.Windows.Forms.FlowDirection.TopDown;
            this._tracksPanel.Location = new System.Drawing.Point(0, 0);
            this._tracksPanel.Name = "_tracksPanel";
            this._tracksPanel.Size = new System.Drawing.Size(800, 450);
            this._tracksPanel.TabIndex = 1;
            // 
            // _newTrackButton
            // 
            this._newTrackButton.Location = new System.Drawing.Point(3, 16);
            this._newTrackButton.Name = "_newTrackButton";
            this._newTrackButton.Size = new System.Drawing.Size(135, 23);
            this._newTrackButton.TabIndex = 1;
            this._newTrackButton.Text = "Record New Track";
            this._newTrackButton.UseVisualStyleBackColor = true;
            this._newTrackButton.Click += new System.EventHandler(this._newTrackButton_Click);
            // 
            // timer1
            // 
            this.timer1.Interval = 20;
            this.timer1.Tick += new System.EventHandler(this.timer1_Tick);
            // 
            // infoLabel
            // 
            this.infoLabel.AutoSize = true;
            this.infoLabel.Location = new System.Drawing.Point(3, 0);
            this.infoLabel.Name = "infoLabel";
            this.infoLabel.Size = new System.Drawing.Size(35, 13);
            this.infoLabel.TabIndex = 2;
            this.infoLabel.Text = "label1";
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(800, 450);
            this.Controls.Add(this._tracksPanel);
            this.Name = "Form1";
            this.Text = "Form1";
            this._tracksPanel.ResumeLayout(false);
            this._tracksPanel.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.FlowLayoutPanel _tracksPanel;
        private System.Windows.Forms.Button _newTrackButton;
        private System.Windows.Forms.Timer timer1;
        private System.Windows.Forms.Label infoLabel;
    }
}

