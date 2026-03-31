//
// Created by zshro on 3/30/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DirectX12Common.h"
#include "RHI/Buffer.h"

namespace carrot::rhi::dx12 {
    class dx12_buffer_t final : public rhi_buffer_t
    {
    public:
        dx12_buffer_t(ID3D12Device* device, const buffer_create_info_t& info);
        ~dx12_buffer_t() override;

        [[nodiscard]] bool write(const void* data, size_t size_bytes, size_t offset_bytes = 0) override;

        [[nodiscard]] ID3D12Resource* resource() const noexcept { return _resource; }

    private:
        ID3D12Resource* _resource{ nullptr };
        void* _mapped_ptr{ nullptr };
        bool _cpu_writable{ false };
    };
} // namespace carrot::rhi::dx12
