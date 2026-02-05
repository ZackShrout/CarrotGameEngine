//
// Created by zshrout on 1/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Renderer.h"

#include "Core/Logger.h"
#include "Window/Window.h"

namespace carrot::renderer {
    // PUBLIC
    void renderer_t::init()
    {
        if (_is_initialized) return;

        LOG_GRAPHICS_INFO("Initializing Renderer...");

        rhi::rhi_desc_t desc{};
        desc.api = rhi::graphics_api::vulkan;
        desc.enable_debug_layers = true;  // change via config later
        desc.width  = window::get_width();
        desc.height = window::get_height();

        _rhi = rhi::create_rhi_context(desc);
        if (!_rhi)
        {
            LOG_GRAPHICS_FATAL("Failed to create RHI context!");
            return;
        }

        create_common_resources();

        _is_initialized = true;
        LOG_GRAPHICS_INFO("Renderer initialized successfully (backend: {})", carrot::rhi::graphics_api_to_string(desc.api));
    }

    void renderer_t::shutdown()
    {
        if (!_is_initialized)
            return;

        LOG_GRAPHICS_INFO("Shutting down Renderer...");

        destroy_common_resources();
        _rhi.reset();

        _is_initialized = false;
        LOG_GRAPHICS_INFO("Renderer shutdown complete");
    }

    void renderer_t::begin_frame()
    {
        _draw_calls_this_frame = 0;
        _frame_index++;

        _rhi->begin_frame();

        // Future place for:
        // - begin main render pass
        // - set common global state (samplers, etc)
    }

    void renderer_t::end_frame()
    {
        // Future place for:
        // - end main render pass
        // - post-process chain
        // - UI overlay
        // - debug overlay

        _rhi->end_frame();
    }

    void renderer_t::draw_fullscreen_colored_triangle(uint32_t abgr_color)
    {
        submit_immediate_triangle(abgr_color);
        _draw_calls_this_frame++;
    }

    void renderer_t::draw_fullscreen_quad(const fullscreen_quad_info_t& info)
    {
        // TODO: real implementation later
        _draw_calls_this_frame++;
        LOG_GRAPHICS_TRACE("Fullscreen quad draw (stub)");
    }

    void renderer_t::draw_sprite(const sprite_draw_info_t& sprite)
    {
        // TODO: real sprite drawing (batch/immediate)
        _draw_calls_this_frame++;
        LOG_GRAPHICS_TRACE("Sprite @ {:.1f},{:.1f}  {}×{}  color:{:08x}",
                           sprite.x, sprite.y, sprite.w, sprite.h, sprite.color);
    }

    void renderer_t::notify_shader_changed(std::string_view path)
    {
        LOG_GRAPHICS_INFO("Shader changed: {}, notifying RHI...", path);
        // For now just log — later forward to pipeline cache / reload system
    }

    // PRIVATE
    void renderer_t::create_common_resources()
    {
        // Place to create:
        // - 1x1 white texture
        // - error/checkerboard texture
        // - common samplers
        // - basic pipelines

        LOG_GRAPHICS_INFO("Common graphics resources created (currently empty)");
    }

    void renderer_t::destroy_common_resources()
    {
        // TODO: Cleanup in reverse order
    }

    void renderer_t::submit_immediate_triangle(uint32_t abgr_color)
    {
        // For now we just log — the actual draw still lives in the hardcoded triangle path
        // Next step: either
        //   A) expose command list from rhi
        //   B) create proper triangle pipeline here with push constant color
        LOG_GRAPHICS_DEBUG("Immediate triangle draw requested (color: {:08x})", abgr_color);
    }
} // namespace carrot::renderer
