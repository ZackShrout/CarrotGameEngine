//
// Created by Zack Shrout on 2/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "MetalSwapchain.h"

#include "MetalLayerBridge.h"

namespace carrot::rhi::metal {
    metal_swapchain_t::metal_swapchain_t([[maybe_unused]] MTL::Device* device, void* ca_metal_layer,
                                         const uint32_t width, const uint32_t height) : _layer{ ca_metal_layer },
                                                                                        _width{ width },
                                                                                        _height{ height }
    {
        CE_ASSERT(_layer && "metal_swapchain_t requires a CAMetalLayer");

        metal_resize_layer(_layer, width, height);
    }

    metal_swapchain_t::~metal_swapchain_t()
    {
        if (_current_drawable)
        {
            metal_release_drawable(_current_drawable);
            _current_drawable = nullptr;
        }

        if (_layer)
        {
            metal_destroy_layer(_layer);
            _layer = nullptr;
        }
    }

    void metal_swapchain_t::resize(const uint32_t width, const uint32_t height)
    {
        _width = width;
        _height = height;

        metal_resize_layer(_layer, width, height);
    }

    uint32_t metal_swapchain_t::acquire_next_image([[maybe_unused]] rhi_semaphore_t* signal_semaphore)
    {
        if (_current_drawable)
        {
            metal_release_drawable(_current_drawable);
            _current_drawable = nullptr;
        }

        _current_drawable = metal_next_drawable(_layer);
        CE_ASSERT(_current_drawable && "Failed to acquire CAMetalDrawable");

        _current_image_index = (_current_image_index + 1) % _image_count;
        return _current_image_index;
    }

    void metal_swapchain_t::present(rhi_semaphore_t* wait_semaphore)
    {
        // Present happens on the command buffer, not here
        // This function exists for API symmetry

        // Drawable is released AFTER command buffer submission
    }

    rhi_texture_t* metal_swapchain_t::get_current_backbuffer() const
    {
        return nullptr;
    }

    uint32_t metal_swapchain_t::get_current_image_index() const
    {
        return _current_image_index;
    }

    uint32_t metal_swapchain_t::get_image_count() const
    {
        return _image_count;
    }

    uint32_t metal_swapchain_t::get_width() const
    {
        return _width;
    }

    uint32_t metal_swapchain_t::get_height() const
    {
        return _height;
    }
} // namespace carrot::rhi::metal
