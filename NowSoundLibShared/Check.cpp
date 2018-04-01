#pragma once

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "Check.h"

namespace NowSound
{
    // Unconditional check that runs whether in debug mode or not.
    void Check(bool condition)
    {
        if (!condition)
        {
            DebugBreak();
            std::abort();
        }
    }
}
