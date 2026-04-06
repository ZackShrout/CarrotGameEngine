//
// Created by Codex on 4/6/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "ControllerManager.h"

namespace carrot::input {
    void controller_manager_t::update_platform_state() noexcept
    {
        reset_raw_gamepads();
    }
} // namespace carrot::input
