/// Copyright by Rob Jellinghaus.  All rights reserved.

namespace NowSoundWinFormsApp
{
    /// <summary>
    /// Constants which are tuned based on subjective feedback and experience.
    /// </summary>
    internal class MagicConstants
    {
        /// <summary>
        /// 2048 is enough to resolve down to about two octaves below middle C (e.g. 65 Hz).
        /// </summary>
        internal const int FftBinSize = 2048;
        /// <summary>
        /// Number of output bins; this can be whatever we want to see, rendering-wise.
        /// </summary>
        internal const int OutputBinCount = 20;
        /// <summary>
        /// Number of divisions per octave (e.g. setting this to 3 equals four semitones per bin, 12 divided by 3).
        /// </summary>
        internal const int OctaveDivisions = 5;
        /// <summary>
        /// The central frequency of the histogram; this is middle C.
        /// </summary>
        internal const float CentralFrequency = 261.626f;
        /// <summary>
        /// The bin (out of OutputBinCount) in which the central frequency should be mapped; zero-indexed.
        /// </summary>
        internal const int CentralFrequencyBin = 10;
    }
}
