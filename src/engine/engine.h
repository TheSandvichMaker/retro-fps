// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#include "core/core.h"

#include "api_types.h"
#include "audio_playback.h"
#include "platform.h"
#include "profiler.h"

#if DF_USE_RHI_ABSTRACTION

#include "rhi/rhi_api.h"

#elif PLATFORM_WIN32

#include "d3d11.h"

#endif
