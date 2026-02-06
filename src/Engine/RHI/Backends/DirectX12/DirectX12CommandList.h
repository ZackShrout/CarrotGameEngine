//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DirectX12Common.h"
#include "RHI/CommandList.h"

namespace carrot::rhi::dx12 {
    class dx12_command_list_t final : rhi_command_list_t
    {
    public:
        explicit dx12_command_list_t(void* device);
        ~dx12_command_list_t() override;

        void reset() override;
        void begin_recording() override;
        void end_recording() override;

        // Accessors for internal use
        [[nodiscard]] ID3D12GraphicsCommandList* id3d12_graphics_command_list() const noexcept { return _cmd_list; }

    private:
        ID3D12CommandAllocator*     _allocator{ nullptr };
        ID3D12GraphicsCommandList*  _cmd_list{ nullptr };
    };
} // namespace carrot::rhi::dx12
