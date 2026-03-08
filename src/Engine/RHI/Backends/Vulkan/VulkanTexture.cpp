//
// Created by zshrout on 3/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "VulkanTexture.h"

#include "VulkanDevice.h"

namespace carrot::rhi::vulkan {
    vulkan_texture_t::vulkan_texture_t(vulkan_device_t* device) : _device{ device } {}

    vulkan_texture_t::~vulkan_texture_t()
    {
        if (_device->vk_device() == VK_NULL_HANDLE)
            return;

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
} // namespace carrot::rhi::vulkan
