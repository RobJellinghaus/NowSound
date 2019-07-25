using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace NowSoundWinFormsApp
{
    public class Utilities
    {
        public static void RenderFrequencyBuffer(float[] fftBuffer, StringBuilder builder)
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
