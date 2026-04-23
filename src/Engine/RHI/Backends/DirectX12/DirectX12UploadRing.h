//
// Created by Zack Shrout.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Memory/Ring.h"
#include "DirectX12Common.h"
#include "RHI/RHI.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace carrot::rhi::dx12 {
    class dx12_buffer_t;

    class dx12_upload_ring_t
    {
    public:
        struct allocation_t
        {
            const rhi_buffer_t* buffer{ nullptr };
            std::byte* mapped_ptr{ nullptr };
            size_t offset_bytes{ 0u };
            size_t size_bytes{ 0u };
            bool wrapped{ false };
        };

        dx12_upload_ring_t(ID3D12Device* device, buffer_usage_t usage, size_t capacity_bytes);
        ~dx12_upload_ring_t();

        DISABLE_COPY(dx12_upload_ring_t)

        [[nodiscard]] std::optional<allocation_t> allocate(size_t size_bytes,
                                                           size_t alignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT) noexcept;
        void reset() noexcept;
        bool ensure_capacity(ID3D12Device* device, size_t required_capacity_bytes) noexcept;

        [[nodiscard]] dx12_buffer_t* buffer() const noexcept { return _buffer.get(); }
        [[nodiscard]] size_t capacity() const noexcept { return _allocator.capacity(); }

    private:
        buffer_usage_t _usage{ buffer_usage_t::staging };
        std::unique_ptr<dx12_buffer_t> _buffer;
        core::memory::ring_t _allocator;
    };
} // namespace carrot::rhi::dx12
