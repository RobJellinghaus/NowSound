﻿#pragma once

// WinRT
#include "ppltasks.h"
#include "winrt/Windows.ApplicationModel.Core.h"
#include "winrt/Windows.Devices.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Media.Devices.h"
#include "winrt/Windows.Media.MediaProperties.h"
#include "winrt/Windows.Storage.Pickers.h"
#include "winrt/Windows.UI.Composition.h"
#include "winrt/Windows.UI.Core.h"
WINRT_WARNING_PUSH

// Windows, COM, WASAPI
#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>
#include <AudioClient.h>
#include <mmdeviceapi.h>

// stdlib
#include "math.h"
#include "stdint.h"
