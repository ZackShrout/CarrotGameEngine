//
// Created by zshrout on 12/28/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::debug {
    void init() noexcept;
    void shutdown() noexcept;
    void render() noexcept; // call every frame after scene

    bool is_initialized() noexcept;

    // Immediate-mode printf-style text
    void text(float x, float y, const char* fmt, ...) noexcept;
} // namespace carrot::debug
