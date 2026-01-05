//
// Created by zshrout on 1/3/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#pragma once

#include "CommandQueue.h"

#include <cstddef>
#include <cstdint>

namespace carrot::rhi {
    class swapchain_t;
    class buffer_t;
    class texture_t;
    class graphics_pipeline_t;

    struct buffer_desc_t
    {
        size_t          size = 0;
        // buffer_usage    usage = buffer_usage::vertex;  // we'll define enum later
        // memory_property memory = memory_property::device_local;
        const void*     initial_data = nullptr;
    };

    class device_t
    {
    public:
        virtual ~device_t() = default;

        virtual command_queue_t*        create_command_queue(queue_type type) = 0;
        virtual swapchain_t*            create_swapchain(void* native_window, uint32_t width, uint32_t height) = 0;

        virtual buffer_t*               create_buffer(const buffer_desc_t& desc) = 0;
        virtual texture_t*              create_texture(/*const texture_desc_t& desc*/) = 0;
        virtual graphics_pipeline_t*    create_graphics_pipeline(/*const graphics_pipeline_desc_t& desc*/) = 0;

        virtual void destroy_buffer(buffer_t* buffer) = 0;

        // ... other destroy/create as needed
    };
} // namespace carrot::rhi