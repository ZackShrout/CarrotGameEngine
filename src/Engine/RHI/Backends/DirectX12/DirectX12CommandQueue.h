//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DirectX12Common.h"
#include "DirectX12Fence.h"
#include "RHI/CommandQueue.h"

namespace carrot::rhi::dx12 {
    class dx12_command_queue_t final : public rhi_command_queue_t
    {
    public:
        explicit dx12_command_queue_t(ID3D12Device* device);
        ~dx12_command_queue_t() override;

        void submit(rhi_command_list_t* cmd_list,
                    rhi_fence_t* fence_to_signal,
                    rhi_semaphore_t* wait_semaphore,
                    rhi_semaphore_t* signal_semaphore) override;

        void wait_idle() override;

        // Accessors for internal use
        [[nodiscard]] ID3D12CommandQueue* id3d12_command_queue() const noexcept { return _queue; }

    private:
        ID3D12CommandQueue* _queue{ nullptr };
        std::unique_ptr<dx12_fence_t> _idle_fence;
    };
} // namespace carrot::rhi::dx12
