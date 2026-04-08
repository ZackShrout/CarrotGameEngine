//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/RHI.h"
#include "Window/Window.h"

#include <cstdint>
#include <vector>

namespace carrot {
    enum class engine_runtime_window_role_t : uint8_t
    {
        gameplay_main = 0,
        gameplay_mirror = 1,
        log_console = 2,
    };

    struct engine_runtime_window_spec_t
    {
        engine_runtime_window_role_t role{ engine_runtime_window_role_t::gameplay_main };
        window::window_create_desc_t create_desc{ };
        bool is_main_window{ false };
        bool register_for_presentation{ false };
        uint32_t presentation_channel_mask{ rhi::presentation_channel_gameplay };
        bool receives_gameplay_input{ false };
    };

    [[nodiscard]] inline std::vector<engine_runtime_window_spec_t> build_engine_runtime_window_specs(
        const uint32_t width,
        const uint32_t height)
    {
        return {
            engine_runtime_window_spec_t{
                .role = engine_runtime_window_role_t::gameplay_main,
                .create_desc = {
                    .width = width,
                    .height = height,
                    .title = "Carrot Engine – Main Window"
                },
                .is_main_window = true,
                .register_for_presentation = false,
                .receives_gameplay_input = true
            },
            engine_runtime_window_spec_t{
                .role = engine_runtime_window_role_t::gameplay_mirror,
                .create_desc = { },
                .is_main_window = false,
                .register_for_presentation = true,
                .presentation_channel_mask = rhi::presentation_channel_gameplay,
                .receives_gameplay_input = false
            },
            engine_runtime_window_spec_t{
                .role = engine_runtime_window_role_t::log_console,
                .create_desc = {
                    .width = 1280,
                    .height = 280,
                    .title = "Carrot Log Console"
                },
                .is_main_window = false,
                .register_for_presentation = true,
                .presentation_channel_mask = rhi::presentation_channel_log_console,
                .receives_gameplay_input = false
            }
        };
    }
} // namespace carrot
