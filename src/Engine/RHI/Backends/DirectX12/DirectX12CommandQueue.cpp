//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DirectX12CommandQueue.h"

#include "DirectX12CommandList.h"
#include "DirectX12Fence.h"

namespace carrot::rhi::dx12 {
    dx12_command_queue_t::dx12_command_queue_t(ID3D12Device* device)
    {
        D3D12_COMMAND_QUEUE_DESC desc{ };
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        HRESULT hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&_queue));
        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to create DX12 command queue");

        _idle_fence = std::make_unique<dx12_fence_t>(device);
        _idle_fence_value = 0;
    }

    dx12_command_queue_t::~dx12_command_queue_t()
    {
        wait_idle();

        if (_queue) _queue->Release();
    }

    void dx12_command_queue_t::submit(rhi_command_list_t* cmd_list, rhi_fence_t* fence_to_signal,
                                      [[maybe_unused]] rhi_semaphore_t* wait_semaphore,
                                      [[maybe_unused]] rhi_semaphore_t* signal_semaphore)
    {
        if (cmd_list)
        {
            const dx12_command_list_t* dx_cmd{ static_cast<dx12_command_list_t *>(cmd_list) };
            ID3D12CommandList* lists[]{ dx_cmd->id3d12_graphics_command_list() };
            _queue->ExecuteCommandLists(1, lists);
        }

        if (fence_to_signal)
        {
            static_cast<dx12_fence_t *>(fence_to_signal)->signal(_queue);
        }
    }

    void dx12_command_queue_t::wait_idle()
    {
        if (!_idle_fence)
            return;

        // Signal + wait this fence to ensure queue is drained
        uint64_t value{ _idle_fence->signal(_queue) };
        _idle_fence->wait(/*timeout_ns=*/~0ULL);
    }
} // namespace carrot::rhi::dx12
