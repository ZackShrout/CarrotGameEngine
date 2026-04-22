//
// Created by Zack Shrout on 2/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/CoreDefines.h"
#include "MetalCommon.h"

namespace carrot::rhi::metal {
    class metal_render_encoder_t final
    {
    public:
        metal_render_encoder_t() = default;
        ~metal_render_encoder_t();

        DISABLE_COPY(metal_render_encoder_t);

        metal_render_encoder_t(metal_render_encoder_t&& other) noexcept : _encoder(other._encoder)
        {
            other._encoder = nullptr;
        }

        metal_render_encoder_t& operator=(metal_render_encoder_t&& other) noexcept
        {
            if (this != &other)
            {
                if (_encoder)
                {
                    _encoder->endEncoding();
                    _encoder->release();
                }

                _encoder = other._encoder;
                other._encoder = nullptr;
            }
            return *this;
        }

        void begin(MTL::CommandBuffer* cmd_buffer, const CA::MetalDrawable* drawable,
                   const MTL::ClearColor& clear_color) noexcept;
        void begin(MTL::CommandBuffer* cmd_buffer, MTL::Texture* texture,
                   const MTL::ClearColor& clear_color) noexcept;
        void begin_with_load_action(MTL::CommandBuffer* cmd_buffer, const CA::MetalDrawable* drawable,
                                    MTL::LoadAction load_action,
                                    const MTL::ClearColor& clear_color) noexcept;
        void begin_with_load_action(MTL::CommandBuffer* cmd_buffer, MTL::Texture* texture,
                                    MTL::LoadAction load_action,
                                    const MTL::ClearColor& clear_color) noexcept;
        void end() noexcept;

        [[nodiscard]] MTL::RenderCommandEncoder* encoder() const noexcept { return _encoder; }

    private:
        MTL::RenderCommandEncoder* _encoder{ nullptr };
    };
} // namespace carrot::rhi::metal
