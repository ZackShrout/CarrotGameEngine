//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DirectX12CommandList.h"

namespace carrot::rhi::dx12 {
    dx12_command_list_t::dx12_command_list_t(ID3D12Device* device, ID3D12CommandAllocator* allocator)
    {
        DX12_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&_cmd_list)));

        DX12_NAME(_cmd_list, L"DX12 Command List");

        DX12_CHECK(_cmd_list->Close());
    }

    dx12_command_list_t::~dx12_command_list_t()
    {
        if (_cmd_list) _cmd_list->Release();
    }

    void dx12_command_list_t::reset()
    {
        if (!_allocator)
            LOG_GRAPHICS_FATAL("DX12 command list reset without allocator");

        DX12_CHECK(_cmd_list->Reset(_allocator, nullptr));
    }

    void dx12_command_list_t::begin_recording()
    {
        // ID3D12GraphicsCommandList enters recording state on Reset().
    }

    void dx12_command_list_t::end_recording()
    {
        DX12_CHECK(_cmd_list->Close());
    }
} // namespace carrot::rhi::dx12
