//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "DirectX12CommandQueue.h"

namespace carrot::rhi::dx12 {
    dx12_command_queue_t::dx12_command_queue_t(void* device)
    {

    }

    dx12_command_queue_t::~dx12_command_queue_t()
    {

    }

    void dx12_command_queue_t::submit(rhi_command_list_t* cmd_list, rhi_fence_t* fence_to_signal,
        rhi_semaphore_t* wait_semaphore, rhi_semaphore_t* signal_semaphore)
    {

    }

    void dx12_command_queue_t::wait_idle()
    {

    }
} // namespace carrot::rhi::dx12
