//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "VulkanCommandQueue.h"

namespace carrot::rhi::vulkan {
    vulkan_command_queue_t::vulkan_command_queue_t(VkQueue queue, uint32_t family_index)
    {

    }

    vulkan_command_queue_t::~vulkan_command_queue_t()
    {

    }

    void vulkan_command_queue_t::submit(rhi_command_list_t* cmd_list, rhi_fence_t* fence_to_signal,
        rhi_semaphore_t* wait_semaphore, rhi_semaphore_t* signal_semaphore)
    {

    }

    void vulkan_command_queue_t::wait_idle()
    {

    }
} // namespace carrot::rhi::vulkan
