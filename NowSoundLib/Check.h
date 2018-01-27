#pragma once

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

namespace NowSound
{
    // Unconditional check that runs whether in debug mode or not.
    void Check(bool condition)
    {
        if (!condition)
        {
#if DEBUG
            Debug::Break();
#endif
            std::abort();
        }
    }
}