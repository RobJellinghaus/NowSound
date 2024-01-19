// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

namespace NowSoundLib
{
    /// <summary>
    /// A time, with float precision.
    /// </summary>
    /// <typeparam name="TTime"></typeparam>
    public struct ContinuousTime<TTime>
    {
        readonly float _value;

        public ContinuousTime(float duration)
        {
            _value = duration;
        }

        public override string ToString()
        {
            return $"CT[{_value:F2}]";
        }

        public static explicit operator float(ContinuousTime<TTime> time)
        {
            return time._value;
        }

        public static implicit operator ContinuousTime<TTime>(float value)
        {
            return new ContinuousTime<TTime>(value);
        }
    }

    /// <summary>
    /// A continous distance between two Times.
    /// </summary>
    /// <typeparam name="TTime"></typeparam>
    public struct ContinuousDuration<TTime>
    {
        readonly float m_duration;

        public ContinuousDuration(float duration)
        {
            m_duration = duration;
        }

        public override string ToString()
        {
            return $"CD[{m_duration:F2}]";
        }

        public static explicit operator float(ContinuousDuration<TTime> duration)
        {
            return duration.m_duration;
        }

        public static implicit operator ContinuousDuration<TTime>(float value)
        {
            return new ContinuousDuration<TTime>(value);
        }

        public static ContinuousDuration<TTime> operator *(ContinuousDuration<TTime> duration, float value)
        {
            return new ContinuousDuration<TTime>(value * duration.m_duration);
        }
        public static ContinuousDuration<TTime> operator *(float value, ContinuousDuration<TTime> duration)
        {
            return new ContinuousDuration<TTime>(value * duration.m_duration);
        }
    }
}
