//
// Created by zshro on 1/24/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

//
// Created by zshrout on 1/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "ShaderWatcher.h"

#include "Core/Logger.h"

#include <filesystem>
#include <cstring>

namespace carrot::hot_reload {
    namespace {
        struct state_t
        {

        } _state;

        constexpr size_t _buffer_size{ 8192 };
    } // anonymous namespace

    void shader_watcher_t::init(const shader_reload_callback_t& callback) noexcept
    {
    }

    void shader_watcher_t::shutdown() noexcept
    {
    }

    void shader_watcher_t::poll() noexcept
    {
    }
} // namespace carrot::hot_reload
