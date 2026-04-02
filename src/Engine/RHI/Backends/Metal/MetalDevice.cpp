//
// Created by Zack Shrout on 2/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "MetalDevice.h"

#include "MetalCommandQueue.h"

namespace carrot::rhi::metal {
    metal_device_t::metal_device_t(MTL::Device* device) : _device{ device }
    {
        // NOTE: User code usually does not release Metal devices. They are either
        //       system-owned or retained by MTKView, so we don't retain/release here.
    }

    metal_device_t::~metal_device_t()
    {
        // Do NOT release _device — it's owned elsewhere (MTKView or system)
    }

    rhi_command_queue_t* metal_device_t::create_command_queue(const queue_type type)
    {
        // For now we only support graphics queue (Metal has unified queues)
        if (type != queue_type::graphics) {
            LOG_GRAPHICS_WARN("Only graphics queue supported on Metal");
            return nullptr;
        }

        MTL::CommandQueue* native_queue{ MTL_CHECK_FATAL(_device->newCommandQueue()) };

        if (!native_queue) return nullptr;

        return new metal_command_queue_t{ native_queue };
    }

    rhi_swapchain_t* metal_device_t::create_swapchain([[maybe_unused]] uint32_t width,
                                                      [[maybe_unused]] uint32_t height)
    {
        return nullptr; // temporary
    }

    rhi_buffer_t* metal_device_t::create_buffer([[maybe_unused]] const buffer_desc_t& desc)
    {
        return nullptr; // temporary
    }

    rhi_texture_t* metal_device_t::create_texture()
    {
        return nullptr; // temporary
    }

    rhi_graphics_pipeline_t* metal_device_t::create_graphics_pipeline()
    {
        return nullptr; // temporary
    }

    void metal_device_t::destroy_buffer([[maybe_unused]] rhi_buffer_t* buffer) {}
} // namespace carrot::rhi::metal
