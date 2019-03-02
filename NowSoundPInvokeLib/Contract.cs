// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using System;
using System.Runtime.CompilerServices;

namespace NowSoundLib
{
    public class ContractException : Exception
    {
        public ContractException(string message) : base(message)
        { }
    }

    public static class Contract
    {
        /// <summary>
        /// Precondition check; if this fails, a method was passed invalid arguments.
        /// </summary>
        public static void Requires(bool condition, [CallerFilePath] string filePath = null, [CallerLineNumber] int sourceLine = 0)
        {
            if (!condition)
            {
                Fail("Contract requirement failure", filePath, sourceLine);
            }
        }

        /// <summary>
        /// Precondition check; if this fails, a method was passed invalid arguments.
        /// </summary>
        public static void RequiresWithMessage(bool condition, string message, [CallerFilePath] string filePath = null, [CallerLineNumber] int sourceLine = 0)
        {
            if (!condition)
            {
                Fail(message, filePath, sourceLine);
            }
        }

        /// <summary>
        /// Standard assertion; if this fails, a method made an invalid calculation.
        /// </summary>
        public static void Assert(bool condition, [CallerFilePath] string filePath = null, [CallerLineNumber] int sourceLine = 0)
        {
            if (!condition)
            {
                Fail("Assertion failed", filePath, sourceLine);
            }
        }

        /// <summary>
        /// Standard assertion; if this fails, a method made an invalid calculation.
        /// </summary>
        public static void Assert(bool condition, string message, [CallerFilePath] string filePath = null, [CallerLineNumber] int sourceLine = 0)
        {
            if (!condition)
            {
                Fail(message, filePath, sourceLine);
            }
        }

        public static void Fail(string message, [CallerFilePath] string filePath = null, [CallerLineNumber] int sourceLine = 0)
        {
            message = $"{message} at {filePath}:{sourceLine}";
#if UNITY
            UnityEngine.Debug.LogError(message);
#endif
            throw new Exception(message);
        }
    }
}
