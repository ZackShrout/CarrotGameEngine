//
// Created by zshrout on 3/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "VulkanCommon.h"
#include "RHI/Texture.h"

namespace carrot::rhi::vulkan {
    class vulkan_device_t;

    class vulkan_texture_t final : public rhi_texture_t
    {
    public:
        explicit vulkan_texture_t(vulkan_device_t* device);
        ~vulkan_texture_t() override;

        [[nodiscard]] uint32_t width() const noexcept override { return _width; }
        [[nodiscard]] uint32_t height() const noexcept override { return _height; }

        [[nodiscard]] VkImage image() const noexcept { return _image; }
        [[nodiscard]] VkImageView view() const noexcept { return _view; }
        [[nodiscard]] VkSampler sampler() const noexcept { return _sampler; }
        [[nodiscard]] texture_format_t format() const noexcept override { return _format; }

        void set_image(VkImage image) noexcept { _image = image; }
        void set_memory(VkDeviceMemory memory) noexcept { _memory = memory; }
        void set_view(VkImageView view) noexcept { _view = view; }
        void set_sampler(VkSampler sampler) noexcept { _sampler = sampler; }
        void set_width(const uint32_t width) noexcept { _width = width; }
        void set_height(const uint32_t height) noexcept { _height = height; }
        void set_format(const texture_format_t format) noexcept { _format = format; }

    private:
        vulkan_device_t* _device{ nullptr };
        VkImage _image{ VK_NULL_HANDLE };
        VkDeviceMemory _memory{ VK_NULL_HANDLE };
        VkImageView _view{ VK_NULL_HANDLE };
        VkSampler _sampler{ VK_NULL_HANDLE };
        uint32_t _width{ 0 };
        uint32_t _height{ 0 };
        texture_format_t _format{ texture_format_t::rgba8_srgb };
    };
} // namespace carrot::rhi::vulkan
