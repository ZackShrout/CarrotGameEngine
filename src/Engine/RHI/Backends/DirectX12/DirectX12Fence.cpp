//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DirectX12Fence.h"

namespace carrot::rhi::dx12 {
    dx12_fence_t::dx12_fence_t(ID3D12Device* device)
    {
        DX12_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));
        DX12_NAME(_fence, L"DX12 Fence");

        _event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!_event)
            LOG_GRAPHICS_FATAL("Failed to create DX12 fence event");
    }

    dx12_fence_t::~dx12_fence_t()
    {
        if (_event) CloseHandle(_event);
        if (_fence) _fence->Release();
    }

    void dx12_fence_t::wait(const uint64_t timeout_ns/* = ~0ULL*/)
    {
        if (_fence->GetCompletedValue() >= _value)
            return;

        DX12_CHECK(_fence->SetEventOnCompletion(_value, _event));

        DWORD timeout_ms{ INFINITE };
        if (timeout_ns != ~0ULL)
            timeout_ms = static_cast<DWORD>(timeout_ns / 1'000'000ULL);

        WaitForSingleObject(_event, timeout_ms);
    }

    void dx12_fence_t::advance()
    {
        ++_value;
    }

    uint64_t dx12_fence_t::signal(ID3D12CommandQueue* queue)
    {
        ++_value;
        DX12_CHECK(queue->Signal(_fence, _value));

        return _value;
    }
} // namespace carrot::rhi::dx12
