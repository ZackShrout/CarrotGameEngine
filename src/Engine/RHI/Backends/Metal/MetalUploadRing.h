//
// Created by Zack Shrout.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Memory/Ring.h"
#include "MetalCommon.h"
#include "RHI/RHI.h"

#include <cstddef>
#include <memory>
#include <optional>

namespace carrot::rhi::metal {
    class metal_buffer_t;

    class metal_upload_ring_t
    {
    public:
        explicit metal_upload_ring_t(buffer_usage_t usage, size_t capacity_bytes) noexcept;
        ~metal_upload_ring_t();

        DISABLE_COPY(metal_upload_ring_t)

        [[nodiscard]] std::optional<transient_upload_allocation_t> allocate(
            size_t size_bytes,
            size_t alignment = alignof(std::max_align_t)) noexcept;
        void reset() noexcept;
        bool ensure_capacity(MTL::Device* device, size_t required_capacity_bytes) noexcept;

        [[nodiscard]] metal_buffer_t* buffer() const noexcept { return _buffer.get(); }
        [[nodiscard]] size_t capacity() const noexcept { return _allocator.capacity(); }

    private:
        buffer_usage_t _usage{ buffer_usage_t::staging };
        std::unique_ptr<metal_buffer_t> _buffer;
        core::memory::ring_t _allocator;
    };
} // namespace carrot::rhi::metal
