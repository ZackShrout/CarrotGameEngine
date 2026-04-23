//
// Created by Zack Shrout.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "MetalUploadRing.h"

#include "MetalBuffer.h"

namespace carrot::rhi::metal {
    metal_upload_ring_t::metal_upload_ring_t(const buffer_usage_t usage, const size_t capacity_bytes) noexcept
        : _usage{ usage }, _allocator{ capacity_bytes } {}

    metal_upload_ring_t::~metal_upload_ring_t() = default;

    std::optional<transient_upload_allocation_t> metal_upload_ring_t::allocate(const size_t size_bytes,
                                                                               const size_t alignment) noexcept
    {
        const auto allocation{ _allocator.allocate(size_bytes, alignment) };
        if (!allocation || !_buffer || !_buffer->mtl_buffer())
            return std::nullopt;

        auto* mapped_ptr{ static_cast<std::byte*>(_buffer->mtl_buffer()->contents()) };
        if (!mapped_ptr)
            return std::nullopt;

        return transient_upload_allocation_t{
            .buffer = _buffer.get(),
            .mapped_ptr = mapped_ptr + allocation->offset_bytes,
            .offset_bytes = allocation->offset_bytes,
            .size_bytes = allocation->size_bytes
        };
    }

    void metal_upload_ring_t::reset() noexcept
    {
        _allocator.reset();
    }

    bool metal_upload_ring_t::ensure_capacity(MTL::Device* const device, const size_t required_capacity_bytes) noexcept
    {
        if (required_capacity_bytes <= capacity() && _buffer && _buffer->mtl_buffer())
            return true;

        const size_t target_capacity{
            std::max(required_capacity_bytes,
                     capacity() > 0u ? capacity() * 2u : required_capacity_bytes)
        };
        MTL::Buffer* backing{ device ? device->newBuffer(target_capacity, MTL::ResourceStorageModeShared) : nullptr };
        if (!backing)
            return false;

        _buffer = std::make_unique<metal_buffer_t>(backing, target_capacity, _usage);
        _allocator = core::memory::ring_t{ target_capacity };
        return true;
    }
} // namespace carrot::rhi::metal
