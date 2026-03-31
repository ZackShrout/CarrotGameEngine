//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DirectX12Common.h"
#include "RHI/Fence.h"

namespace carrot::rhi::dx12 {
    class dx12_fence_t final : public rhi_fence_t
    {
    public:
        explicit dx12_fence_t(ID3D12Device* device);
        ~dx12_fence_t() override;

        void wait(uint64_t timeout_ns = ~0ULL) override;
        void advance() override;

        uint64_t signal(ID3D12CommandQueue* queue);

        // Accessors for internal use
        [[nodiscard]] ID3D12Fence* id3d12_fence() const noexcept { return _fence; }
        [[nodiscard]] uint64_t current_value() const noexcept { return _value; }

    private:
        ID3D12Fence*    _fence{ nullptr };
        HANDLE          _event{ nullptr };
        uint64_t        _value{ 0 };
    };
} // namespace carrot::rhi::dx12
