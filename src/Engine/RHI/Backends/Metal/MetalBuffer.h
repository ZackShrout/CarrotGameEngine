//
// Created by Zack Shrout on 3/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#pragma once

#include "MetalCommon.h"
#include "RHI/Buffer.h"

namespace carrot::rhi::metal {
    class metal_buffer_t final : public rhi_buffer_t
    {
    public:
        metal_buffer_t(MTL::Buffer* buffer, size_t size_bytes, buffer_usage_t usage) noexcept;
        ~metal_buffer_t() override;

        [[nodiscard]] bool write(const void* data, size_t size_bytes, size_t offset_bytes = 0) override;

        [[nodiscard]] MTL::Buffer* mtl_buffer() const noexcept { return _buffer; }

    private:
        MTL::Buffer* _buffer{ nullptr };
    };
} // namespace carrot::rhi::metal
