//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "DirectX12Fence.h"

namespace carrot::rhi::dx12 {
    dx12_fence_t::dx12_fence_t(ID3D12Device* device)
    {
        HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to create DX12 fence");

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

        HRESULT hr{ _fence->SetEventOnCompletion(_value, _event) };
        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to set DX12 fence completion event");

        DWORD timeout_ms;

        if (timeout_ns == ~0ULL)
            timeout_ms = INFINITE;
        else
            timeout_ms = static_cast<DWORD>(timeout_ns / 1'000'000ULL);

        WaitForSingleObject(_event, timeout_ms);
    }

    void dx12_fence_t::reset()
    {
        ++_value;
    }

    uint64_t dx12_fence_t::signal(ID3D12CommandQueue* queue)
    {
        ++_value;
        queue->Signal(_fence, _value);
        return _value;
    }
} // namespace carrot::rhi::dx12
