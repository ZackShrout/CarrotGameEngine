//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Fence.h"

namespace carrot::rhi::dx12 {
    class dx12_fence_t final : rhi_fence_t
    {
    public:
        explicit dx12_fence_t(void* device);
        ~dx12_fence_t() override;

        void wait(uint64_t timeout_ns) override;
        void reset() override;

        // Accessors for internal use
        [[nodiscard]] void* id3d12_fence() const noexcept { return _fence; }

    private:
        void*     _fence{ nullptr }; // ID3D12Fence*
        uint64_t  _value{ 0 };
    };
} // namespace carrot::rhi::dx12
