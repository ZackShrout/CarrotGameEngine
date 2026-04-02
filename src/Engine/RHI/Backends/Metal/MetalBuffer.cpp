//
// Created by Zack Shrout on 3/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "MetalBuffer.h"

namespace carrot::rhi::metal {
    metal_buffer_t::metal_buffer_t(MTL::Buffer* buffer, const size_t size_bytes,
                                   const buffer_usage_t usage) noexcept
        : rhi_buffer_t{ size_bytes, usage }, _buffer{ buffer } {}

    metal_buffer_t::~metal_buffer_t()
    {
        if (_buffer)
            _buffer->release();
    }

    bool metal_buffer_t::write(const void* data, const size_t size_bytes, const size_t offset_bytes/* = 0*/)
    {
        if (!_buffer || !data)
            return false;

        if ((offset_bytes + size_bytes) > this->size_bytes())
            return false;

        std::memcpy(static_cast<uint8_t*>(_buffer->contents()) + offset_bytes, data, size_bytes);

        if (_buffer->storageMode() == MTL::StorageModeManaged)
            _buffer->didModifyRange(NS::Range(offset_bytes, size_bytes));

        return true;
    }
} // namespace carrot::rhi::metal
