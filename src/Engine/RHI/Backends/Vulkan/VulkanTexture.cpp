//
// Created by zshrout on 3/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "VulkanTexture.h"

#include "VulkanDevice.h"

namespace carrot::rhi::vulkan {
    vulkan_texture_t::vulkan_texture_t(vulkan_device_t* device) : _device{ device } {}

    vulkan_texture_t::~vulkan_texture_t()
    {
        if (_device->vk_device() == VK_NULL_HANDLE) return;

        if (_sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(_device->vk_device(), _sampler, nullptr);
            _sampler = VK_NULL_HANDLE;
        }

        if (_view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(_device->vk_device(), _view, nullptr);
            _view = VK_NULL_HANDLE;
        }

        if (_image != VK_NULL_HANDLE)
        {
            vkDestroyImage(_device->vk_device(), _image, nullptr);
            _image = VK_NULL_HANDLE;
        }

        if (_memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(_device->vk_device(), _memory, nullptr);
            _memory = VK_NULL_HANDLE;
        }
    }

    vulkan_render_target_t::~vulkan_render_target_t()
    {
        if (!_device || _device->vk_device() == VK_NULL_HANDLE)
            return;

        if (_clear_framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(_device->vk_device(), _clear_framebuffer, nullptr);
            _clear_framebuffer = VK_NULL_HANDLE;
        }

        if (_load_framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(_device->vk_device(), _load_framebuffer, nullptr);
            _load_framebuffer = VK_NULL_HANDLE;
        }
    }

    uint32_t vulkan_render_target_t::width() const noexcept
    {
        return _color_texture ? _color_texture->width() : 0u;
    }

    uint32_t vulkan_render_target_t::height() const noexcept
    {
        return _color_texture ? _color_texture->height() : 0u;
    }

    texture_format_t vulkan_render_target_t::format() const noexcept
    {
        return _color_texture ? _color_texture->format() : texture_format_t::rgba8_srgb;
    }
} // namespace carrot::rhi::vulkan
