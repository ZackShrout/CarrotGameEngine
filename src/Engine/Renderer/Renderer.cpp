//
// Created by zshrout on 1/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Renderer.h"

#include "Assets/AssetManager.h"
#include "Assets/AssetService.h"
#include "Assets/Texture/TextureAsset.h"
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

        _is_initialized = true;
        LOG_GRAPHICS_INFO("Renderer initialized successfully (backend: {})",
                          carrot::rhi::graphics_api_to_string(desc.api));
    }

    void renderer_t::shutdown()
    {
        if (!_is_initialized)
            return;

        LOG_GRAPHICS_INFO("Shutting down Renderer...");

        if (_rhi)
            _rhi->wait_idle();

        destroy_common_resources();
        _rhi.reset();

        _is_initialized = false;
        LOG_GRAPHICS_INFO("Renderer shutdown complete");
    }

    void renderer_t::init_common_resources()
    {
        create_common_resources();
    }

    void renderer_t::begin_frame()
    {
        _draw_calls_this_frame = 0;
        _frame_index++;

        _textured_quad_vertices_cpu.clear();
        _textured_quad_indices_cpu.clear();
        _textured_quad_batches.clear();

        _rhi->begin_frame();

        // Future place for:
        // - begin main render pass
        // - set common global state (samplers, etc)

        textured_quad_draw_info_t quad1{ };
        quad1.texture = _test_texture;
        quad1.x = -0.9f;
        quad1.y = -0.9f;
        quad1.width = 0.4f;
        quad1.height = 0.4f;
        quad1.color = 0xFFFFFFFFu;
        draw_textured_quad(quad1);

        textured_quad_draw_info_t quad2{ };
        quad2.texture = _test_texture;
        quad2.x = -0.2f;
        quad2.y = -0.2f;
        quad2.width = 0.4f;
        quad2.height = 0.4f;
        quad2.color = 0xFFFFFFFFu;
        draw_textured_quad(quad2);

        textured_quad_draw_info_t quad3{ };
        quad3.texture = _test_texture;
        quad3.x = 0.5f;
        quad3.y = 0.5f;
        quad3.width = 0.3f;
        quad3.height = 0.3f;
        quad3.color = 0xFFFFFFFFu;
        draw_textured_quad(quad3);
    }

    void renderer_t::record_frame()
    {
        _draw_calls_this_frame = static_cast<uint32_t>(_textured_quad_batches.size());

        if (!_textured_quad_batches.empty())
        {
            upload_textured_quad_frame_data();

            if (_textured_quad_frame_vertex_buffer && _textured_quad_frame_index_buffer)
            {
                _rhi->set_textured_quad_geometry(
                    *_textured_quad_frame_vertex_buffer,
                    *_textured_quad_frame_index_buffer
                );

                _rhi->set_textured_quad_batches(_textured_quad_batches);
            }
        }

        _rhi->record_frame();
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

    void renderer_t::draw_textured_quad(const textured_quad_draw_info_t& quad)
    {
        if (quad.texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_textured_quad called with null texture");
            return;
        }

        if (_textured_quad_batches.empty() || _textured_quad_batches.back().texture != quad.texture)
        {
            _textured_quad_batches.push_back(textured_quad_batch_t{
                .texture = quad.texture,
                .first_index = static_cast<uint32_t>(_textured_quad_indices_cpu.size()),
                .index_count = 0
            });
        }

        const uint32_t base_vertex{ static_cast<uint32_t>(_textured_quad_vertices_cpu.size()) };

        _textured_quad_vertices_cpu.push_back(quad_vertex_t{
            .x = quad.x,
            .y = quad.y,
            .u = quad.u0,
            .v = quad.v0,
            .color = quad.color
        });

        _textured_quad_vertices_cpu.push_back(quad_vertex_t{
            .x = quad.x + quad.width,
            .y = quad.y,
            .u = quad.u1,
            .v = quad.v0,
            .color = quad.color
        });

        _textured_quad_vertices_cpu.push_back(quad_vertex_t{
            .x = quad.x + quad.width,
            .y = quad.y + quad.height,
            .u = quad.u1,
            .v = quad.v1,
            .color = quad.color
        });

        _textured_quad_vertices_cpu.push_back(quad_vertex_t{
            .x = quad.x,
            .y = quad.y + quad.height,
            .u = quad.u0,
            .v = quad.v1,
            .color = quad.color
        });

        _textured_quad_indices_cpu.push_back(base_vertex + 0);
        _textured_quad_indices_cpu.push_back(base_vertex + 1);
        _textured_quad_indices_cpu.push_back(base_vertex + 2);
        _textured_quad_indices_cpu.push_back(base_vertex + 0);
        _textured_quad_indices_cpu.push_back(base_vertex + 2);
        _textured_quad_indices_cpu.push_back(base_vertex + 3);

        _textured_quad_batches.back().index_count += 6;
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
        _test_texture = nullptr;
        _quad_vertex_buffer.reset();
        _quad_index_buffer.reset();

        const assets::loaded_texture_asset_t* botan_texture{
            assets::asset_service_t::manager().textures().get("engine.botan_test")
        };

        if (botan_texture == nullptr || !botan_texture->valid())
        {
            LOG_GRAPHICS_ERROR("Failed to load texture asset 'engine.botan_test'");
            return;
        }

        _test_texture = botan_texture->texture.get();

        LOG_GRAPHICS_INFO(
            "Loaded texture asset '{}' successfully",
            botan_texture->record->logical_id
        );

        rhi::buffer_create_info_t vertex_buffer_info{ };
        vertex_buffer_info.size_bytes = sizeof(k_unit_quad_vertices);
        vertex_buffer_info.usage = rhi::buffer_usage_t::vertex;
        vertex_buffer_info.initial_data = k_unit_quad_vertices.data();

        _quad_vertex_buffer = _rhi->create_buffer(vertex_buffer_info);
        if (!_quad_vertex_buffer)
        {
            LOG_GRAPHICS_FATAL("Failed to create canonical quad vertex buffer");
            return;
        }

        rhi::buffer_create_info_t index_buffer_info{ };
        index_buffer_info.size_bytes = sizeof(k_unit_quad_indices);
        index_buffer_info.usage = rhi::buffer_usage_t::index;
        index_buffer_info.initial_data = k_unit_quad_indices.data();

        _quad_index_buffer = _rhi->create_buffer(index_buffer_info);
        if (!_quad_index_buffer)
        {
            LOG_GRAPHICS_FATAL("Failed to create canonical quad index buffer");
            return;
        }

        LOG_GRAPHICS_INFO(
            "Common graphics resources created: quad VB={} bytes, quad IB={} bytes",
            _quad_vertex_buffer->size_bytes(),
            _quad_index_buffer->size_bytes()
        );

        _rhi->set_textured_quad_geometry(*_quad_vertex_buffer, *_quad_index_buffer);
    }

    void renderer_t::destroy_common_resources()
    {
        _textured_quad_vertices_cpu.clear();
        _textured_quad_indices_cpu.clear();
        _textured_quad_batches.clear();

        _quad_index_buffer.reset();
        _quad_vertex_buffer.reset();
        _test_texture = nullptr;

        _textured_quad_frame_vertex_buffer.reset();
        _textured_quad_frame_index_buffer.reset();
        _textured_quad_vertex_capacity = 0;
        _textured_quad_index_capacity = 0;
    }

    void renderer_t::submit_immediate_triangle(uint32_t abgr_color)
    {
        // For now we just log — the actual draw still lives in the hardcoded triangle path
        // Next step: either
        //   A) expose command list from rhi
        //   B) create proper triangle pipeline here with push constant color
        LOG_GRAPHICS_DEBUG("Immediate triangle draw requested (color: {:08x})", abgr_color);
    }

    void renderer_t::ensure_textured_quad_frame_buffers()
    {
        const size_t required_vertex_bytes = _textured_quad_vertices_cpu.size() * sizeof(quad_vertex_t);
        const size_t required_index_bytes = _textured_quad_indices_cpu.size() * sizeof(uint32_t);

        if (required_vertex_bytes == 0 || required_index_bytes == 0)
            return;

        if (_textured_quad_frame_vertex_buffer == nullptr || _textured_quad_vertex_capacity < required_vertex_bytes)
        {
            _textured_quad_frame_vertex_buffer.reset();

            rhi::buffer_create_info_t info{ };
            info.size_bytes = required_vertex_bytes;
            info.usage = rhi::buffer_usage_t::vertex;
            info.initial_data = _textured_quad_vertices_cpu.data();

            _textured_quad_frame_vertex_buffer = _rhi->create_buffer(info);
            _textured_quad_vertex_capacity = required_vertex_bytes;
        }

        if (_textured_quad_frame_index_buffer == nullptr || _textured_quad_index_capacity < required_index_bytes)
        {
            _textured_quad_frame_index_buffer.reset();

            rhi::buffer_create_info_t info{ };
            info.size_bytes = required_index_bytes;
            info.usage = rhi::buffer_usage_t::index;
            info.initial_data = _textured_quad_indices_cpu.data();

            _textured_quad_frame_index_buffer = _rhi->create_buffer(info);
            _textured_quad_index_capacity = required_index_bytes;
        }
    }

    void renderer_t::upload_textured_quad_frame_data()
    {
        if (_textured_quad_vertices_cpu.empty() || _textured_quad_indices_cpu.empty())
            return;

        rhi::buffer_create_info_t vertex_info{ };
        vertex_info.size_bytes = _textured_quad_vertices_cpu.size() * sizeof(quad_vertex_t);
        vertex_info.usage = rhi::buffer_usage_t::vertex;
        vertex_info.initial_data = _textured_quad_vertices_cpu.data();

        _textured_quad_frame_vertex_buffer = _rhi->create_buffer(vertex_info);

        rhi::buffer_create_info_t index_info{ };
        index_info.size_bytes = _textured_quad_indices_cpu.size() * sizeof(uint32_t);
        index_info.usage = rhi::buffer_usage_t::index;
        index_info.initial_data = _textured_quad_indices_cpu.data();

        _textured_quad_frame_index_buffer = _rhi->create_buffer(index_info);
    }
} // namespace carrot::renderer
