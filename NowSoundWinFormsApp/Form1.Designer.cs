﻿// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using NowSoundLib;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
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

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this._tracksPanel = new System.Windows.Forms.FlowLayoutPanel();
            this.infoLabel = new System.Windows.Forms.Label();
            this.inputPanel0 = new System.Windows.Forms.Panel();
            this.flowLayoutPanel0 = new System.Windows.Forms.FlowLayoutPanel();
            this.inputPanel1 = new System.Windows.Forms.Panel();
            this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
            this._newTrackButton = new System.Windows.Forms.Button();
            this.timer1 = new System.Windows.Forms.Timer(this.components);
            this.recordToFileButton = new System.Windows.Forms.Button();
            this._tracksPanel.SuspendLayout();
            this.inputPanel0.SuspendLayout();
            this.inputPanel1.SuspendLayout();
            this.SuspendLayout();
            // 
            // _tracksPanel
            // 
            this._tracksPanel.Controls.Add(this.infoLabel);
            this._tracksPanel.Controls.Add(this.inputPanel0);
            this._tracksPanel.Controls.Add(this.inputPanel1);
            this._tracksPanel.Controls.Add(this._newTrackButton);
            this._tracksPanel.Controls.Add(this.recordToFileButton);
            this._tracksPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._tracksPanel.FlowDirection = System.Windows.Forms.FlowDirection.TopDown;
            this._tracksPanel.Location = new System.Drawing.Point(0, 0);
            this._tracksPanel.Name = "_tracksPanel";
            this._tracksPanel.Size = new System.Drawing.Size(1121, 466);
            this._tracksPanel.TabIndex = 1;
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
            // inputPanel0
            // 
            this.inputPanel0.Controls.Add(this.flowLayoutPanel0);
            this.inputPanel0.Location = new System.Drawing.Point(3, 16);
            this.inputPanel0.Name = "inputPanel0";
            this.inputPanel0.Size = new System.Drawing.Size(1106, 28);
            this.inputPanel0.TabIndex = 3;
            // 
            // flowLayoutPanel0
            // 
            this.flowLayoutPanel0.Dock = System.Windows.Forms.DockStyle.Fill;
            this.flowLayoutPanel0.Location = new System.Drawing.Point(0, 0);
            this.flowLayoutPanel0.Name = "flowLayoutPanel0";
            this.flowLayoutPanel0.Size = new System.Drawing.Size(1106, 28);
            this.flowLayoutPanel0.TabIndex = 0;
            // 
            // inputPanel1
            // 
            this.inputPanel1.Controls.Add(this.flowLayoutPanel1);
            this.inputPanel1.Location = new System.Drawing.Point(3, 50);
            this.inputPanel1.Name = "inputPanel1";
            this.inputPanel1.Size = new System.Drawing.Size(1106, 28);
            this.inputPanel1.TabIndex = 4;
            // 
            // flowLayoutPanel1
            // 
            this.flowLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.flowLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            this.flowLayoutPanel1.Name = "flowLayoutPanel1";
            this.flowLayoutPanel1.Size = new System.Drawing.Size(1106, 28);
            this.flowLayoutPanel1.TabIndex = 0;
            // 
            // _newTrackButton
            // 
            this._newTrackButton.Location = new System.Drawing.Point(3, 84);
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
            // recordToFileButton
            // 
            this.recordToFileButton.Location = new System.Drawing.Point(3, 113);
            this.recordToFileButton.Name = "recordToFileButton";
            this.recordToFileButton.Size = new System.Drawing.Size(135, 23);
            this.recordToFileButton.TabIndex = 5;
            this.recordToFileButton.Text = "Record Audio To File";
            this.recordToFileButton.UseVisualStyleBackColor = true;
            this.recordToFileButton.Click += new System.EventHandler(this.RecordToFileButton_Click);
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1121, 466);
            this.Controls.Add(this._tracksPanel);
            this.Name = "Form1";
            this.Text = "Form1";
            this._tracksPanel.ResumeLayout(false);
            this._tracksPanel.PerformLayout();
            this.inputPanel0.ResumeLayout(false);
            this.inputPanel1.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.FlowLayoutPanel _tracksPanel;
        private System.Windows.Forms.Button _newTrackButton;
        private System.Windows.Forms.Timer timer1;
        private System.Windows.Forms.Label infoLabel;
        private System.Windows.Forms.Panel inputPanel0;
        private System.Windows.Forms.Panel inputPanel1;
        private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel0;
        private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
        private System.Windows.Forms.Button recordToFileButton;
    }
}

