//
// Created by zshrout on 1/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Renderer.h"

#include "Assets/Image/ImageAssetImporter.h"
#include "IO/VirtualFileSystem.h"
#include "Primitives/QuadMesh.h"
#include "Utils/File/FileUtils.h"
#include "Window/Window.h"
#include "RHI/Texture.h"
#include "RHI/Buffer.h"

namespace carrot::renderer {
    // PUBLIC

    renderer_t::renderer_t(io::virtual_file_system_t& vfs, const engine_graphics_config_t& config)
        : _vfs{ vfs }, _config{ config }
    {
        init();
    }

    void renderer_t::init()
    {
        if (_is_initialized) return;

        LOG_GRAPHICS_INFO("Initializing Renderer...");

        _shader_provider = std::make_unique<assets::vfs_shader_file_provider_t>(_vfs);

        rhi::rhi_desc_t desc{ };
        desc.api = _config.api;
        desc.enable_debug_layers = _config.enable_debug_layers;
        desc.width = window::get_width();
        desc.height = window::get_height();
        desc.shader_files = _shader_provider.get();

        _rhi = rhi::create_rhi_context(desc);
        if (!_rhi)
        {
            LOG_GRAPHICS_FATAL("Failed to create RHI context!");
            return;
        }

        create_common_resources();

        _is_initialized = true;
        LOG_GRAPHICS_INFO("Renderer initialized successfully (backend: {})",
                          carrot::rhi::graphics_api_to_string(desc.api));
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

        // LOG_GRAPHICS_INFO("Common graphics resources created (currently empty)");

        const assets::image_load_result_t image_result{
            assets::load_image_rgba8(utils::file::resolve_asset_path("assets/images/16x16orange.png"))
        };

        if (!image_result.success())
        {
            LOG_CORE_ERROR("Failed to load PNG: {}", assets::to_string(image_result.error));
            return;
        }

        rhi::texture_create_info_t texture_info{ };
        texture_info.width = image_result.image.width;
        texture_info.height = image_result.image.height;
        texture_info.format = image_result.image.is_srgb
                                  ? rhi::texture_format_t::rgba8_srgb
                                  : rhi::texture_format_t::rgba8_unorm;
        texture_info.initial_data = image_result.image.data();
        texture_info.initial_data_size = image_result.image.size_bytes();
        texture_info.initial_data_stride_bytes = image_result.image.stride_bytes;

        _test_texture = _rhi->create_texture_2d(texture_info);

        if (!_test_texture)
        {
            LOG_GRAPHICS_ERROR("Failed to create demo texture from PNG");
            return;
        }

        LOG_GRAPHICS_INFO("Created demo texture: {}x{}",
                          _test_texture->width(),
                          _test_texture->height());

        rhi::buffer_create_info_t vertex_buffer_info{ };
        vertex_buffer_info.size_bytes = sizeof(k_unit_quad_vertices);
        vertex_buffer_info.usage = rhi::buffer_usage_t::vertex;
        vertex_buffer_info.initial_data = k_unit_quad_vertices.data();

        _quad_vertex_buffer = _rhi->create_buffer(vertex_buffer_info);
        if (!_quad_vertex_buffer)
            LOG_GRAPHICS_FATAL("Failed to create canonical quad vertex buffer");

        rhi::buffer_create_info_t index_buffer_info{ };
        index_buffer_info.size_bytes = sizeof(k_unit_quad_indices);
        index_buffer_info.usage = rhi::buffer_usage_t::index;
        index_buffer_info.initial_data = k_unit_quad_indices.data();

        _quad_index_buffer = _rhi->create_buffer(index_buffer_info);
        if (!_quad_index_buffer)
            LOG_GRAPHICS_FATAL("Failed to create canonical quad index buffer");

        LOG_GRAPHICS_INFO(
            "Common graphics resources created: quad VB={} bytes, quad IB={} bytes",
            _quad_vertex_buffer->size_bytes(),
            _quad_index_buffer->size_bytes()
        );
    }

    void renderer_t::destroy_common_resources()
    {
        _quad_index_buffer.reset();
        _quad_vertex_buffer.reset();
        _test_texture.reset();
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
