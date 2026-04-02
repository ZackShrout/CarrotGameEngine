//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/CommandQueue.h"
#include "VulkanCommon.h"

namespace carrot::rhi::vulkan {

    class vulkan_command_queue_t final : public rhi_command_queue_t
    {
    public:
        explicit vulkan_command_queue_t(VkQueue queue, uint32_t family_index);
        ~vulkan_command_queue_t() override;

        void submit(rhi_command_list_t* cmd_list,
                    rhi_fence_t* fence_to_signal = nullptr,
                    rhi_semaphore_t* wait_semaphore = nullptr,
                    rhi_semaphore_t* signal_semaphore = nullptr) override;

        void wait_idle() override;

        [[nodiscard]] VkQueue vk_queue() const noexcept { return _queue; }

    private:
        VkQueue  _queue{ VK_NULL_HANDLE };
        uint32_t _family_index{ ~0u };
    };

} // namespace carrot::rhi::vulkan
