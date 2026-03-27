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
        CE_ASSERT(!_encoder && "Render encoder already active");

        MTL::RenderPassDescriptor* rpd{ MTL::RenderPassDescriptor::alloc()->init() };

        MTL::RenderPassColorAttachmentDescriptor* color{ rpd->colorAttachments()->object(0) };
        color->setTexture(drawable->texture());
        color->setLoadAction(MTL::LoadActionClear);
        color->setStoreAction(MTL::StoreActionStore);
        color->setClearColor(clear_color);

        _encoder = cmd_buffer->renderCommandEncoder(rpd);
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
