//
// Created by zshrout on 12/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::renderer {
    class renderer_t;
}

namespace carrot::debug {
    void init(renderer::renderer_t* renderer, const io::virtual_file_system_t& vfs) noexcept;
    void shutdown() noexcept;

    [[nodiscard]] bool is_initialized() noexcept;

    // Immediate-mode printf-style text. Step 1 uses the current 2D world/camera
    // convention so text appears in the authored top-left camera space.
    void text(float x, float y, const char* fmt, ...) noexcept;
} // namespace carrot::debug
