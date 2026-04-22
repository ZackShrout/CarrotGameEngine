//
// Created by Zack Shrout on 2/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "MetalRenderEncoder.h"

namespace carrot::rhi::metal {
    metal_render_encoder_t::~metal_render_encoder_t()
    {
        if (_encoder)
        {
            _encoder->endEncoding();
            _encoder->release();
            _encoder = nullptr;
        }
    }

    void metal_render_encoder_t::begin(MTL::CommandBuffer* cmd_buffer, const CA::MetalDrawable* drawable,
        const MTL::ClearColor& clear_color) noexcept
    {
        begin_with_load_action(cmd_buffer, drawable, MTL::LoadActionClear, clear_color);
    }

    void metal_render_encoder_t::begin(MTL::CommandBuffer* cmd_buffer, MTL::Texture* texture,
        const MTL::ClearColor& clear_color) noexcept
    {
        begin_with_load_action(cmd_buffer, texture, MTL::LoadActionClear, clear_color);
    }

    void metal_render_encoder_t::begin_with_load_action(MTL::CommandBuffer* cmd_buffer,
        const CA::MetalDrawable* drawable,
        const MTL::LoadAction load_action,
        const MTL::ClearColor& clear_color) noexcept
    {
        begin_with_load_action(cmd_buffer, drawable ? drawable->texture() : nullptr, load_action, clear_color);
    }

    void metal_render_encoder_t::begin_with_load_action(MTL::CommandBuffer* cmd_buffer,
        MTL::Texture* texture,
        const MTL::LoadAction load_action,
        const MTL::ClearColor& clear_color) noexcept
    {
        CE_ASSERT(!_encoder && "Render encoder already active");

        if (!cmd_buffer || !texture)
            return;

        MTL::RenderPassDescriptor* rpd{ MTL::RenderPassDescriptor::alloc()->init() };

        MTL::RenderPassColorAttachmentDescriptor* color{ rpd->colorAttachments()->object(0) };
        color->setTexture(texture);
        color->setLoadAction(load_action);
        color->setStoreAction(MTL::StoreActionStore);
        color->setClearColor(clear_color);

        _encoder = cmd_buffer->renderCommandEncoder(rpd);
        if (_encoder)
            _encoder->retain();

        rpd->release();
    }

    void metal_render_encoder_t::end() noexcept
    {
        if (!_encoder) return;

        _encoder->endEncoding();
        _encoder->release();
        _encoder = nullptr;
    }
} // namespace carrot::rhi::metal
