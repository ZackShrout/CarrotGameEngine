//
// Created by Zack Shrout.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DirectX12UploadRing.h"
#include "DirectX12Buffer.h"

namespace carrot::rhi::dx12 {
    dx12_upload_ring_t::dx12_upload_ring_t(ID3D12Device* const device,
                                           const buffer_usage_t usage,
                                           const size_t capacity_bytes)
        : _usage{ usage }, _allocator{ capacity_bytes }
    {
        if (!device)
            LOG_GRAPHICS_FATAL("DX12 upload ring created with null device");

        if (capacity_bytes == 0u)
            LOG_GRAPHICS_FATAL("DX12 upload ring created with zero capacity");
        const buffer_create_info_t info{
            .size_bytes = capacity_bytes,
            .usage = usage,
            .initial_data = nullptr,
            .cpu_writable = true
        };
        _buffer = std::make_unique<dx12_buffer_t>(device, info);
        if (!_buffer || !_buffer->resource() || !_buffer->mapped_ptr())
            LOG_GRAPHICS_FATAL("DX12 upload ring failed to create backing buffer");
    }

    dx12_upload_ring_t::~dx12_upload_ring_t() = default;

    std::optional<dx12_upload_ring_t::allocation_t> dx12_upload_ring_t::allocate(const size_t size_bytes,
                                                                                  const size_t alignment) noexcept
    {
        const auto alloc{ _allocator.allocate(size_bytes, alignment) };
        if (!alloc || !_buffer || !_buffer->resource() || !_buffer->mapped_ptr())
            return std::nullopt;

        return allocation_t{
            .buffer = _buffer.get(),
            .mapped_ptr = static_cast<std::byte*>(const_cast<void*>(_buffer->mapped_ptr())) + alloc->offset_bytes,
            .offset_bytes = alloc->offset_bytes,
            .size_bytes = alloc->size_bytes,
            .wrapped = alloc->wrapped
        };
    }

    void dx12_upload_ring_t::reset() noexcept
    {
        _allocator.reset();
    }

    bool dx12_upload_ring_t::ensure_capacity(ID3D12Device* const device, const size_t required_capacity_bytes) noexcept
    {
        if (required_capacity_bytes <= capacity() && _buffer && _buffer->resource() && _buffer->mapped_ptr())
            return true;

        const size_t target_capacity{
            std::max(required_capacity_bytes,
                     capacity() > 0u ? capacity() * 2u : required_capacity_bytes)
        };
        const buffer_create_info_t info{
            .size_bytes = target_capacity,
            .usage = _usage,
            .initial_data = nullptr,
            .cpu_writable = true
        };
        auto replacement{ std::make_unique<dx12_buffer_t>(device, info) };
        if (!replacement || !replacement->resource() || !replacement->mapped_ptr())
            return false;

        _buffer = std::move(replacement);
        _allocator = core::memory::ring_t{ target_capacity };
        return true;
    }
} // namespace carrot::rhi::dx12
