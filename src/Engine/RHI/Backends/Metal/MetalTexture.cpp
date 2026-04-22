//
// Created by Zack Shrout on 3/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "MetalTexture.h"

namespace carrot::rhi::metal {
    metal_texture_t::metal_texture_t(MTL::Texture* texture, const uint32_t width, const uint32_t height,
                                     const texture_format_t format) noexcept
        : _texture{ texture }, _width{ width }, _height{ height }, _format{ format } {}

    metal_texture_t::~metal_texture_t()
    {
        if (_texture)
            _texture->release();
    }

    uint32_t metal_render_target_t::width() const noexcept
    {
        return _color_texture ? _color_texture->width() : 0u;
    }

    uint32_t metal_render_target_t::height() const noexcept
    {
        return _color_texture ? _color_texture->height() : 0u;
    }

    texture_format_t metal_render_target_t::format() const noexcept
    {
        return _color_texture ? _color_texture->format() : texture_format_t::rgba8_srgb;
    }
} // namespace carrot::rhi::metal
