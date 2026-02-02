//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Input/PlatformKeyMapping.h"

#include <AppKit/Appkit.h>

namespace carrot::core::platform {
    inline uint8_t translate_modifier_flags(NSEventModifierFlags cocoaFlags) noexcept
    {
        uint8_t mods = 0;

        if (cocoaFlags & NSEventModifierFlagShift) mods |= static_cast<uint8_t>(input::modifier::shift);
        if (cocoaFlags & NSEventModifierFlagControl) mods |= static_cast<uint8_t>(input::modifier::control);
        if (cocoaFlags & NSEventModifierFlagOption) mods |= static_cast<uint8_t>(input::modifier::alt);
        if (cocoaFlags & NSEventModifierFlagCommand) mods |= static_cast<uint8_t>(input::modifier::super);

        return mods;
    }
} // namespace
