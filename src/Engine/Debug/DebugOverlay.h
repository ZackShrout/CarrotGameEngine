//
// Created by zshrout on 12/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

#include "Collision/CollisionWorld.h"

#include <cstdint>
#include <span>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::renderer {
    class renderer_t;
}

namespace carrot::debug {
    struct world_rect_style_t
    {
        uint32_t color{ 0xFF00FF00u };
        float outline_thickness{ 2.f };
        bool filled{ false };
    };

    void init(renderer::renderer_t* renderer, const io::virtual_file_system_t& vfs) noexcept;
    void shutdown() noexcept;

    [[nodiscard]] bool is_initialized() noexcept;

    // Immediate-mode printf-style text. Step 1 uses the current 2D world/camera
    // convention so text appears in the authored top-left camera space.
    void text_sized(float x, float y, float font_pixel_height, const char* fmt, ...) noexcept;
    void text_colored_sized(float x, float y, float font_pixel_height, uint32_t color, const char* fmt, ...) noexcept;
    void text(float x, float y, const char* fmt, ...) noexcept;
    void text_colored(float x, float y, uint32_t color, const char* fmt, ...) noexcept;
    void log_console_text(float x, float y, const char* fmt, ...) noexcept;
    void log_console_text_colored(float x, float y, uint32_t color, const char* fmt, ...) noexcept;
    void world_rect(float x, float y, float width, float height, world_rect_style_t style = {}) noexcept;
    void world_aabb(const collision::collision_aabb_t& bounds, world_rect_style_t style = {}) noexcept;
    void world_polygon(std::span<const chlm::float2> points, world_rect_style_t style = {}) noexcept;
} // namespace carrot::debug
