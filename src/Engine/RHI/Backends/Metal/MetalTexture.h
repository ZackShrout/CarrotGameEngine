//
// Created by Zack Shrout on 3/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "MetalCommon.h"
#include "RHI/Texture.h"

namespace carrot::rhi::metal {
    class metal_texture_t final : public rhi_texture_t
    {
    public:
        metal_texture_t(MTL::Texture* texture, uint32_t width, uint32_t height, texture_format_t format) noexcept;
        ~metal_texture_t() override;

        [[nodiscard]] uint32_t width() const noexcept override { return _width; }
        [[nodiscard]] uint32_t height() const noexcept override { return _height; }
        [[nodiscard]] texture_format_t format() const noexcept override { return _format; }
        [[nodiscard]] bool has_initial_data() const noexcept override { return _has_initial_data; }

        [[nodiscard]] MTL::Texture* mtl_texture() const noexcept { return _texture; }
        void set_has_initial_data(const bool has_initial_data) noexcept { _has_initial_data = has_initial_data; }

    private:
        MTL::Texture*    _texture{ nullptr };
        uint32_t         _width{ 0 };
        uint32_t         _height{ 0 };
        texture_format_t _format{ texture_format_t::rgba8_srgb };
        bool             _has_initial_data{ false };
    };
} // namespace carrot::rhi::metal
