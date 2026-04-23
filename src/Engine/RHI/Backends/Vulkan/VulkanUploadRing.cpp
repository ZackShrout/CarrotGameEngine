//
// Created by Zack Shrout.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "VulkanUploadRing.h"

#include "VulkanBuffer.h"
#include "VulkanRHIContext.h"

namespace carrot::rhi::vulkan {
    vulkan_upload_ring_t::vulkan_upload_ring_t(const buffer_usage_t usage, const size_t capacity_bytes) noexcept
        : _usage{ usage }, _allocator{ capacity_bytes } {}

    vulkan_upload_ring_t::~vulkan_upload_ring_t() = default;

    std::optional<transient_upload_allocation_t> vulkan_upload_ring_t::allocate(const size_t size_bytes,
                                                                                const size_t alignment) noexcept
    {
        const auto allocation{ _allocator.allocate(size_bytes, alignment) };
        if (!allocation || !_buffer)
            return std::nullopt;

        const auto* vk_buffer{ dynamic_cast<const vulkan_buffer_t*>(_buffer.get()) };
        if (!vk_buffer || !vk_buffer->mapped_ptr())
            return std::nullopt;

        return transient_upload_allocation_t{
            .buffer = _buffer.get(),
            .mapped_ptr = static_cast<std::byte*>(vk_buffer->mapped_ptr()) + allocation->offset_bytes,
            .offset_bytes = allocation->offset_bytes,
            .size_bytes = allocation->size_bytes
        };
    }

    void vulkan_upload_ring_t::reset() noexcept
    {
        _allocator.reset();
    }

    bool vulkan_upload_ring_t::ensure_capacity(vulkan_rhi_context_t& context,
                                               const size_t required_capacity_bytes) noexcept
    {
        if (required_capacity_bytes <= capacity() && _buffer)
            return true;

        const size_t target_capacity{
            std::max(required_capacity_bytes,
                     capacity() > 0u ? capacity() * 2u : required_capacity_bytes)
        };
        _buffer = context.create_mapped_upload_buffer(_usage, target_capacity);
        _allocator = core::memory::ring_t{ target_capacity };
        return _buffer != nullptr;
    }

    vulkan_buffer_t* vulkan_upload_ring_t::buffer() const noexcept
    {
        return dynamic_cast<vulkan_buffer_t*>(_buffer.get());
    }
} // namespace carrot::rhi::vulkan
