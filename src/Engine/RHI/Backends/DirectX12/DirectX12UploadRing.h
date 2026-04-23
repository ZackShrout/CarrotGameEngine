//
// Created by Zack Shrout.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Memory/Ring.h"
#include "DirectX12Common.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace carrot::rhi::dx12 {
    class dx12_upload_ring_t
    {
    public:
        struct allocation_t
        {
            ID3D12Resource* resource{ nullptr };
            std::byte* mapped_ptr{ nullptr };
            size_t offset_bytes{ 0u };
            size_t size_bytes{ 0u };
            bool wrapped{ false };
        };

        dx12_upload_ring_t(ID3D12Device* device, size_t capacity_bytes);
        ~dx12_upload_ring_t();

        DISABLE_COPY(dx12_upload_ring_t)

        [[nodiscard]] std::optional<allocation_t> allocate(size_t size_bytes,
                                                           size_t alignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT) noexcept;
        void reset() noexcept;

        [[nodiscard]] ID3D12Resource* resource() const noexcept { return _resource; }
        [[nodiscard]] size_t capacity() const noexcept { return _allocator.capacity(); }

    private:
        ID3D12Resource* _resource{ nullptr };
        std::byte* _mapped_ptr{ nullptr };
        core::memory::ring_t _allocator;
    };
} // namespace carrot::rhi::dx12
