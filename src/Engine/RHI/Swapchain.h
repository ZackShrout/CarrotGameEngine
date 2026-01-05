//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::rhi {
    class texture_t;
    class semaphore_t;

    class swapchain_t
    {
        virtual ~swapchain_t() = default;

        // Called when window is resized
        virtual void resize(uint32_t width, uint32_t height) = 0;

        // Acquire next backbuffer image
        virtual uint32_t acquire_next_image(semaphore_t* signal_semaphore) = 0;

        // Present the current image
        virtual void present(semaphore_t* wait_semaphore) = 0;

        [[nodiscard]] virtual texture_t*    get_current_backbuffer() const = 0;
        [[nodiscard]] virtual uint32_t      get_current_image_index() const = 0;
        [[nodiscard]] virtual uint32_t      get_image_count() const = 0;
        [[nodiscard]] virtual uint32_t      get_width() const = 0;
        [[nodiscard]] virtual uint32_t      get_height() const = 0;
        // [[nodiscard]] virtual format        get_format() const = 0;
    };
} // namespace carrot::rhi