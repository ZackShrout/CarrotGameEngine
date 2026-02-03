//
// Created by Zack Shrout on 2/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "MetalCommon.h"
#include "RHI/CommandQueue.h"

namespace carrot::rhi::metal {
    class metal_command_queue_t final : public rhi_command_queue_t
    {
    public:
        explicit metal_command_queue_t(MTL::CommandQueue* queue);
        ~metal_command_queue_t() override;

        void submit(rhi_command_list_t* cmd_list,
                    rhi_fence_t* fence_to_signal = nullptr,
                    rhi_semaphore_t* wait_semaphore  = nullptr,
                    rhi_semaphore_t* signal_semaphore = nullptr) override;

        void wait_idle() override;

        [[nodiscard]] MTL::CommandQueue* mtl_command_queue() const { return _queue; }

    private:
        MTL::CommandQueue* _queue { nullptr };
    };

} // namespace carrot::rhi::metal
