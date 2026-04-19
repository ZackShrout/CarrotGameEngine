//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DirectX12Common.h"
#include "RHI/Device.h"

namespace carrot::rhi {
    struct rhi_desc_t;
}

namespace carrot::rhi::dx12 {
    class dx12_device_t final : public rhi_device_t
    {
    public:
        explicit dx12_device_t(const rhi_desc_t& desc);
        ~dx12_device_t() override;

        // Accessors for internal use
        [[nodiscard]] ID3D12Device* id3d12_device() const noexcept { return _device; }

    private:
        ID3D12Device* _device{ nullptr };
    };
} // namespace carrot::rhi::dx12
