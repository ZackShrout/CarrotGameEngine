//
// Created by zshrout on 1/3/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#pragma once

#include "VulkanCommon.h"

#include <array>

namespace carrot::rhi::vulkan {
    constexpr uint32_t k_max_frames_in_flight{ 2 };

    struct frame_resources_t
    {
        VkCommandBuffer command_buffer{ VK_NULL_HANDLE };
        VkSemaphore     image_available{ VK_NULL_HANDLE };
        VkSemaphore     render_finished{ VK_NULL_HANDLE };
        VkFence         in_flight{ VK_NULL_HANDLE };
    };

    using frame_data_t = std::array<frame_resources_t, k_max_frames_in_flight>;
} // namespace carrot::rhi::vulkan