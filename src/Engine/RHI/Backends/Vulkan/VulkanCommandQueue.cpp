//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "VulkanCommandQueue.h"

namespace carrot::rhi::vulkan {
    vulkan_command_queue_t::vulkan_command_queue_t(VkQueue queue, uint32_t family_index)
        : _queue{ queue }, _family_index{ family_index }
    {
    }

    vulkan_command_queue_t::~vulkan_command_queue_t()
    {

    }

    void vulkan_command_queue_t::submit([[maybe_unused]] rhi_command_list_t* cmd_list,
                                        [[maybe_unused]] rhi_fence_t* fence_to_signal,
                                        [[maybe_unused]] rhi_semaphore_t* wait_semaphore,
                                        [[maybe_unused]] rhi_semaphore_t* signal_semaphore)
    {
        LOG_GRAPHICS_TRACE("Vulkan submit called on queue family {} (stub)", _family_index);
    }

    void vulkan_command_queue_t::wait_idle()
    {
        if (_queue != VK_NULL_HANDLE)
            vkQueueWaitIdle(_queue);
    }
} // namespace carrot::rhi::vulkan
