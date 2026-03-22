//
// Created by zshrout on 1/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Renderer.h"

#include "Assets/AssetService.h"
#include "Assets/Image/ImageAssetImporter.h"
#include "IO/VirtualFileSystem.h"
#include "Primitives/QuadMesh.h"
#include "Primitives/TexturedQuadPushConstants.h"
#include "Utils/File/FileUtils.h"
#include "Window/Window.h"
#include "RHI/Texture.h"
#include "RHI/Buffer.h"

namespace carrot::renderer {
    namespace {
        [[nodiscard]] constexpr float channel_to_float(const uint32_t value) noexcept
        {
            return static_cast<float>(value) / 255.f;
        }

        struct unpacked_color_t
        {
            float r;
            float g;
            float b;
            float a;
        };

        [[nodiscard]] unpacked_color_t unpack_abgr(const uint32_t abgr) noexcept
        {
            const uint32_t a{ abgr >> 24 & 0xFFu };
            const uint32_t b{ abgr >> 16 & 0xFFu };
            const uint32_t g{ abgr >> 8 & 0xFFu };
            const uint32_t r{ abgr >> 0 & 0xFFu };

            return unpacked_color_t{
                .r = channel_to_float(r),
                .g = channel_to_float(g),
                .b = channel_to_float(b),
                .a = channel_to_float(a)
            };
        }
    } // anonymous namespace

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
        _pending_textured_quad.reset();

        _rhi->begin_frame();

        // Future place for:
        // - begin main render pass
        // - set common global state (samplers, etc)

        textured_quad_draw_info_t quad{ };
        quad.texture = _test_texture;
        quad.x = -0.5f;
        quad.y = -0.5f;
        quad.width = 1.0f;
        quad.height = 1.0f;
        // quad.u0 = 0.0f;
        // quad.v0 = 0.0f;
        // quad.u1 = 0.5f;
        // quad.v1 = 1.0f;
        quad.color = 0xFFFFFFFFu;
        // quad.color = 0xFF0000FFu; // ABGR => red
        // quad.color = 0xFFC0CBFFu; // AGBR => pink
        // quad.color = 0xFFFF0000u; // AGBR => blue
        // quad.color = 0xFFFF00FFu; // AGBR => purple
        // quad.color = 0x80FFFFFFu;

        draw_textured_quad(quad);
    }

    void renderer_t::record_frame()
    {
        if (_pending_textured_quad.has_value())
        {
            const textured_quad_draw_info_t& quad{ *_pending_textured_quad };

            if (quad.texture != _current_textured_quad_texture)
            {
                _rhi->set_textured_quad_texture(*quad.texture);
                _current_textured_quad_texture = quad.texture;
            }

            const unpacked_color_t tint{ unpack_abgr(quad.color) };

            const textured_quad_push_constants_t push_constants{
                .offset_x = quad.x,
                .offset_y = quad.y,
                .scale_x = quad.width,
                .scale_y = quad.height,

                .uv_min_x = quad.u0,
                .uv_min_y = quad.v0,
                .uv_max_x = quad.u1,
                .uv_max_y = quad.v1,

                .tint_r = tint.r,
                .tint_g = tint.g,
                .tint_b = tint.b,
                .tint_a = tint.a
            };

            _rhi->set_textured_quad_push_constants(push_constants);
            _draw_calls_this_frame = 1;
        }
        else
        {
            _draw_calls_this_frame = 0;
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

        _pending_textured_quad = quad;
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

        if (_test_texture != nullptr)
            _rhi->set_textured_quad_texture(*_test_texture);
    }

    void renderer_t::destroy_common_resources()
    {
        _quad_index_buffer.reset();
        _quad_vertex_buffer.reset();
        _test_texture = nullptr;
        _current_textured_quad_texture = nullptr;
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
