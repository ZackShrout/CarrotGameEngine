//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "MetalRHIContext.h"

#include "Window/Window.h"

#include <QuartzCore/CAMetalLayer.h>

namespace carrot::rhi::metal {
    metal_rhi_context_t::metal_rhi_context_t(const rhi_desc_t& desc)
    {
        core::platform::native_window_handle_t window_handle = window::get_primary_window().get_native_handle();
        _view = static_cast<MTK::View*>(window_handle.cocoa_t.mtk_view);

        if (!_view)
        {
            LOG_GRAPHICS_FATAL("No CAMetalLayer set on view!");
            return;
        }

        _device = _view->device();
        if (!_device)
        {
            LOG_GRAPHICS_FATAL("MTKView has no Metal device!");
            return;
        }

        _commandQueue = _device->newCommandQueue();
        if (!_commandQueue)
        {
            LOG_GRAPHICS_FATAL("Failed to create Metal command queue!");
            return;
        }

        LOG_GRAPHICS_INFO("Metal RHI context created successfully");
        LOG_GRAPHICS_INFO("Device: {}", _device->name()->utf8String());
    }

    metal_rhi_context_t::~metal_rhi_context_t()
    {

    }

    void metal_rhi_context_t::begin_frame()
    {

    }

    void metal_rhi_context_t::record_frame()
    {

    }

    void metal_rhi_context_t::end_frame()
    {
        CA::MetalDrawable* drawable = _view->currentDrawable();
        if (!drawable) return;

        MTL::RenderPassDescriptor* rpd = _view->currentRenderPassDescriptor();
        if (!rpd) return;

        // Override the clear color here — should take precedence
        auto* colorAttachment = rpd->colorAttachments()->object(0);
        colorAttachment->setLoadAction(MTL::LoadActionClear);
        colorAttachment->setClearColor(MTL::ClearColor(
            100.0 / 255.0,   // ≈0.392
            149.0 / 255.0,   // ≈0.584
            237.0 / 255.0,   // ≈0.929
            1.0
        ));

        MTL::CommandBuffer* cmdBuf = _commandQueue->commandBuffer();

        MTL::RenderCommandEncoder* encoder = cmdBuf->renderCommandEncoder(rpd);
        if (encoder)
        {
            encoder->endEncoding();
        }

        cmdBuf->presentDrawable(drawable);
        cmdBuf->commit();
    }

    void metal_rhi_context_t::resize(uint32_t width, uint32_t height)
    {

    }

    rhi_device_t* metal_rhi_context_t::get_device() const noexcept
    {
        return nullptr;
    }

    rhi_swapchain_t* metal_rhi_context_t::get_swapchain() const noexcept
    {
        return nullptr;
    }

    rhi_command_queue_t* metal_rhi_context_t::get_command_queue() const noexcept
    {
        return nullptr;
    }

    void metal_rhi_context_t::wait_idle()
    {

    }
} // namespace carrot::rhi::metal