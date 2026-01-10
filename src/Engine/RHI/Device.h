//
// Created by zshrout on 1/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "CommandQueue.h"

#include <cstddef>
#include <cstdint>

namespace carrot::rhi {
    class rhi_swapchain_t;
    class rhi_buffer_t;
    class rhi_texture_t;
    class rhi_graphics_pipeline_t;

    struct buffer_desc_t
    {
        size_t          size = 0;
        // buffer_usage    usage = buffer_usage::vertex;  // we'll define enum later
        // memory_property memory = memory_property::device_local;
        const void*     initial_data = nullptr;
    };

    class rhi_device_t
    {
    public:
        virtual ~rhi_device_t() = default;

        virtual rhi_command_queue_t*        create_command_queue(queue_type type) = 0;
        virtual rhi_swapchain_t*            create_swapchain(void* native_window, uint32_t width, uint32_t height) = 0;

        virtual rhi_buffer_t*               create_buffer(const buffer_desc_t& desc) = 0;
        virtual rhi_texture_t*              create_texture(/*const texture_desc_t& desc*/) = 0;
        virtual rhi_graphics_pipeline_t*    create_graphics_pipeline(/*const graphics_pipeline_desc_t& desc*/) = 0;

        virtual void destroy_buffer(rhi_buffer_t* buffer) = 0;

        // ... other destroy/create as needed
    };
} // namespace carrot::rhi