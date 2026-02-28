//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "DirectX12CommandList.h"

namespace carrot::rhi::dx12 {
    dx12_command_list_t::dx12_command_list_t(ID3D12Device* device, ID3D12CommandAllocator* allocator)
    {
        HRESULT hr{ device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator,
        nullptr,
        IID_PPV_ARGS(&_cmd_list)) };

        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to create DX12 command list");

        _cmd_list->Close();
    }

    dx12_command_list_t::~dx12_command_list_t()
    {
        if (_cmd_list) _cmd_list->Release();
    }

    void dx12_command_list_t::reset()
    {
        if (!_allocator)
            LOG_GRAPHICS_FATAL("DX12 command list reset without allocator");

        HRESULT hr{ _cmd_list->Reset(_allocator, nullptr) };
        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to reset DX12 command list");
    }

    void dx12_command_list_t::begin_recording()
    {

    }

    void dx12_command_list_t::end_recording()
    {
        HRESULT hr{ _cmd_list->Close() };
        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to close DX12 command list");
    }
} // namespace carrot::rhi::dx12
