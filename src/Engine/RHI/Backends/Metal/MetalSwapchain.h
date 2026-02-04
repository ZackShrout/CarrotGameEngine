//
// Created by Zack Shrout on 2/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "MetalCommon.h"
#include "RHI/Swapchain.h"

namespace carrot::rhi::metal {
    class metal_swapchain_t final : public rhi_swapchain_t
    {
    public:
        metal_swapchain_t(MTL::Device* device, void* ca_metal_layer, uint32_t width, uint32_t height);
        ~metal_swapchain_t() override;

        void resize(uint32_t width, uint32_t height) override;

        uint32_t acquire_next_image(rhi_semaphore_t* signal_semaphore) override;
        void present(rhi_semaphore_t* wait_semaphore) override;

        [[nodiscard]] rhi_texture_t* get_current_backbuffer() const override;
        [[nodiscard]] uint32_t get_current_image_index() const override;
        [[nodiscard]] uint32_t get_image_count() const override;
        [[nodiscard]] uint32_t get_width() const override;
        [[nodiscard]] uint32_t get_height() const override;

        void* get_current_drawable() const { return _current_drawable; }

    private:
        void* _layer{ nullptr }; // CAMetalLayer*
        void* _current_drawable{ nullptr }; // CAMetalDrawable*
        // std::unique_ptr<metal_texture_t> _current_texture;

        uint32_t _width{ 0 };
        uint32_t _height{ 0 };

        uint32_t _image_count{ 3 }; // pretend triple buffering
        uint32_t _current_image_index{ 0 };
    };
} // namespace carrot::rhi::metal
