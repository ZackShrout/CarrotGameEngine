//
// Created by Zack Shrout.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Memory/Ring.h"
#include "RHI/RHI.h"

#include <cstddef>
#include <memory>
#include <optional>

namespace carrot::rhi::vulkan {
    class vulkan_rhi_context_t;
    class vulkan_buffer_t;

    class vulkan_upload_ring_t
    {
    public:
        explicit vulkan_upload_ring_t(buffer_usage_t usage, size_t capacity_bytes) noexcept;
        ~vulkan_upload_ring_t();

        DISABLE_COPY(vulkan_upload_ring_t)

        [[nodiscard]] std::optional<transient_upload_allocation_t> allocate(
            size_t size_bytes,
            size_t alignment = alignof(std::max_align_t)) noexcept;
        void reset() noexcept;
        bool ensure_capacity(vulkan_rhi_context_t& context, size_t required_capacity_bytes) noexcept;

        [[nodiscard]] vulkan_buffer_t* buffer() const noexcept;
        [[nodiscard]] size_t capacity() const noexcept { return _allocator.capacity(); }

    private:
        buffer_usage_t _usage{ buffer_usage_t::staging };
        std::unique_ptr<rhi_buffer_t> _buffer;
        core::memory::ring_t _allocator;
    };
} // namespace carrot::rhi::vulkan
