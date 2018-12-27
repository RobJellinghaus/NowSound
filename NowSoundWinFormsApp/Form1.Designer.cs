// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using NowSoundLib;
using System;
using System.Diagnostics;
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
        /// This thread polls for a given audio state.
        /// </summary>
        private Thread _audioPollThread; 

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        protected override async void OnLoad(EventArgs e)
        {
            base.OnLoad(e);

            // let's see when this gets called
            Console.WriteLine("OnLoad");

            // and let's P/Invoke up in here!
            NowSoundGraphAPI.Initialize();

            // No longer necessary really since JUCE initialization is synchronous.
            // JUCETODO: clean this up eventually.
            bool reachedState = await AwaitAudioGraphState(NowSoundGraphState.GraphInitialized, timeoutMsec: 10);

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
            this.label1 = new System.Windows.Forms.Label();
            this.panel1 = new System.Windows.Forms.Panel();
            this.panel1.SuspendLayout();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Dock = System.Windows.Forms.DockStyle.Top;
            this.label1.Location = new System.Drawing.Point(0, 0);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(25, 13);
            this.label1.TabIndex = 0;
            this.label1.Text = "Foo";
            // 
            // panel1
            // 
            this.panel1.Controls.Add(this.label1);
            this.panel1.Dock = System.Windows.Forms.DockStyle.Top;
            this.panel1.Location = new System.Drawing.Point(0, 0);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(800, 100);
            this.panel1.TabIndex = 1;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(800, 450);
            this.Controls.Add(this.panel1);
            this.Name = "Form1";
            this.Text = "Form1";
            this.panel1.ResumeLayout(false);
            this.panel1.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Panel panel1;
    }
}

