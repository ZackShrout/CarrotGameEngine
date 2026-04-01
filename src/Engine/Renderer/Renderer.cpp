//
// Created by zshrout on 1/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Renderer.h"

#include "Assets/Tilemap/LoadedTilemapAsset.h"
#include "Assets/Sprite/LoadedSpriteAsset.h"
#include "Assets/Texture/TextureAsset.h"
#include "Draw/SpriteDrawInfo.h"
#include "IO/VirtualFileSystem.h"
#include "Window/Window.h"
#include "RHI/Buffer.h"
#include "RHI/Swapchain.h"

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
                          carrot::rhi::graphics_api_to_string(_rhi->get_graphics_api()));
    }

    void renderer_t::shutdown()
    {
        if (!_is_initialized)
            return;

        LOG_GRAPHICS_INFO("Shutting down Renderer...");

        if (_rhi)
            _rhi->wait_idle();

        release_frame_resources();

        _shader_provider.reset();
        _rhi.reset();

        _is_initialized = false;
        LOG_GRAPHICS_INFO("Renderer shutdown complete");
    }

    void renderer_t::begin_frame()
    {
        _frame_index++;

        _textured_quad.submissions.clear();
        _textured_quad.vertices_cpu.clear();
        _textured_quad.indices_cpu.clear();
        _textured_quad.batches.clear();

        _stats = { };

        _rhi->begin_frame();
    }

    void renderer_t::end_frame()
    {
        build_textured_quad_batches();

        if (!_textured_quad.batches.empty())
        {
            ensure_textured_quad_frame_buffers();
            upload_textured_quad_frame_data();

            if (_textured_quad.frame_vertex_buffer && _textured_quad.frame_index_buffer)
            {
                _rhi->set_textured_quad_geometry(*_textured_quad.frame_vertex_buffer,
                                                 *_textured_quad.frame_index_buffer);

                _rhi->set_textured_quad_batches(_textured_quad.batches);
            }
        }

        const chlm::uint2 render_target_size{ current_render_target_size() };
        const resolved_camera_2d_t resolved_camera{ _active_camera.resolve(render_target_size) };

        _rhi->set_textured_quad_view_projection(_active_camera.view_projection_matrix(render_target_size));
        _rhi->set_textured_quad_viewport({
            .rect_px = resolved_camera.viewport_rect_px
        });
        _rhi->record_frame();

        _stats.vertex_count = static_cast<uint32_t>(_textured_quad.vertices_cpu.size());
       _stats.index_count = static_cast<uint32_t>(_textured_quad.indices_cpu.size());
        _stats.textured_quad_batch_count = static_cast<uint32_t>(_textured_quad.batches.size());
        _stats.draw_calls = _stats.textured_quad_batch_count;
        _last_completed_stats = _stats;

        _rhi->end_frame();
    }

    void renderer_t::draw_textured_quad(const textured_quad_draw_info_t& quad)
    {
        if (quad.texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_textured_quad called with null texture");
            return;
        }

        _textured_quad.submissions.push_back({
            .quad = quad,
            .submission_index = static_cast<uint64_t>(_textured_quad.submissions.size())
        });

        _stats.textured_quad_count++;
    }

    void renderer_t::draw_sprite(const sprite_draw_info_t& info)
    {
        if (!info.sprite)
        {
            LOG_GRAPHICS_WARN("renderer_t::draw_sprite called with null sprite.");
            return;
        }

        if (!info.frame)
        {
            LOG_GRAPHICS_WARN("renderer_t::draw_sprite called with null frame.");
            return;
        }

        const assets::loaded_texture_asset_t* texture_asset{ info.sprite->texture() };
        if (!texture_asset || !texture_asset->texture)
        {
            LOG_GRAPHICS_WARN("renderer_t::draw_sprite called with sprite missing loaded texture.");
            return;
        }

        const float texture_width{ static_cast<float>(texture_asset->texture->width()) };
        const float texture_height{ static_cast<float>(texture_asset->texture->height()) };

        if (texture_width <= 0.f || texture_height <= 0.f)
        {
            LOG_GRAPHICS_WARN("renderer_t::draw_sprite encountered invalid texture dimensions ({}x{}).",
                              texture_width, texture_height);
            return;
        }

        const chlm::uint_rect& rect{ info.frame->pixel_rect };

        const float u0{ static_cast<float>(rect.position.x) / texture_width };
        const float v0{ static_cast<float>(rect.position.y) / texture_height };
        const float u1{ static_cast<float>(rect.position.x + rect.size.x) / texture_width };
        const float v1{ static_cast<float>(rect.position.y + rect.size.y) / texture_height };
        const chlm::float2 pivot{ info.use_custom_pivot ? info.pivot : info.frame->pivot };
        const float final_u0{ info.flip_x ? u1 : u0 };
        const float final_v0{ info.flip_y ? v1 : v0 };
        const float final_u1{ info.flip_x ? u0 : u1 };
        const float final_v1{ info.flip_y ? v0 : v1 };

        textured_quad_draw_info_t quad{ };
        quad.texture = texture_asset->texture.get();
        quad.x = info.x - (pivot.x * info.width);
        quad.y = info.y - (pivot.y * info.height);
        quad.width = info.width;
        quad.height = info.height;
        quad.layer = info.layer;
        quad.order_in_layer = info.order_in_layer;
        quad.color = info.color;
        quad.sampler_preset = info.sampler_preset;
        quad.u0 = final_u0;
        quad.v0 = final_v0;
        quad.u1 = final_u1;
        quad.v1 = final_v1;

        draw_textured_quad(quad);
    }

    void renderer_t::draw_tilemap(const tilemap_draw_info_t& info)
    {
        if (!info.tilemap || !info.tilemap->valid())
        {
            LOG_GRAPHICS_WARN("renderer_t::draw_tilemap called with invalid tilemap.");
            return;
        }

        const assets::tilemap_asset_t& tilemap{ info.tilemap->tilemap() };
        const auto& tilesets{ tilemap.tilesets() };
        const auto& textures{ info.tilemap->tileset_textures() };

        for (const assets::tilemap_layer_t& layer : tilemap.layers())
        {
            if (!layer.visible || layer.kind != assets::tilemap_layer_kind_t::tile)
                continue;

            for (uint32_t row{ 0 }; row < layer.height; ++row)
            {
                for (uint32_t col{ 0 }; col < layer.width; ++col)
                {
                    const uint32_t cell_index{ row * layer.width + col };
                    if (cell_index >= layer.gids.size())
                        continue;

                    const uint32_t gid{ layer.gids[cell_index] };
                    if (gid == 0)
                        continue;

                    size_t tileset_index{ static_cast<size_t>(-1) };
                    for (size_t i{ 0 }; i < tilesets.size(); ++i)
                    {
                        const uint32_t first_gid{ tilesets[i].first_gid };
                        const uint32_t next_first_gid{
                            (i + 1u) < tilesets.size() ? tilesets[i + 1u].first_gid : std::numeric_limits<uint32_t>::max()
                        };

                        if (gid >= first_gid && gid < next_first_gid)
                        {
                            tileset_index = i;
                            break;
                        }
                    }

                    if (tileset_index == static_cast<size_t>(-1) ||
                        tileset_index >= textures.size() ||
                        !textures[tileset_index])
                    {
                        continue;
                    }

                    const assets::tilemap_tileset_t& tileset{ tilesets[tileset_index] };
                    if (tileset.columns == 0 || tileset.tile_width == 0 || tileset.tile_height == 0 ||
                        tileset.image_width == 0 || tileset.image_height == 0)
                    {
                        continue;
                    }

                    const uint32_t local_tile_index{ gid - tileset.first_gid };
                    const uint32_t tile_u_index{ local_tile_index % tileset.columns };
                    const uint32_t tile_v_index{ local_tile_index / tileset.columns };

                    const float u0{ static_cast<float>(tile_u_index * tileset.tile_width) /
                                    static_cast<float>(tileset.image_width) };
                    const float v0{ static_cast<float>(tile_v_index * tileset.tile_height) /
                                    static_cast<float>(tileset.image_height) };
                    const float u1{ static_cast<float>((tile_u_index + 1u) * tileset.tile_width) /
                                    static_cast<float>(tileset.image_width) };
                    const float v1{ static_cast<float>((tile_v_index + 1u) * tileset.tile_height) /
                                    static_cast<float>(tileset.image_height) };

                    textured_quad_draw_info_t tile_quad{ };
                    tile_quad.texture = textures[tileset_index].get();
                    tile_quad.x = info.origin.x + (static_cast<float>(col) * static_cast<float>(tilemap.tile_width()));
                    tile_quad.y = info.origin.y + (static_cast<float>(row) * static_cast<float>(tilemap.tile_height()));
                    tile_quad.width = static_cast<float>(tilemap.tile_width());
                    tile_quad.height = static_cast<float>(tilemap.tile_height());
                    tile_quad.u0 = u0;
                    tile_quad.v0 = v0;
                    tile_quad.u1 = u1;
                    tile_quad.v1 = v1;
                    tile_quad.layer = info.layer;
                    tile_quad.order_in_layer = info.order_in_layer;
                    tile_quad.color = info.color;
                    tile_quad.sampler_preset = info.sampler_preset;

                    draw_textured_quad(tile_quad);
                }
            }
        }
    }

    void renderer_t::notify_shader_changed(std::string_view path)
    {
        LOG_GRAPHICS_INFO("Shader changed: {}, notifying RHI...", path);
        // For now just log — later forward to pipeline cache / reload system
    }

    // PRIVATE

    chlm::uint2 renderer_t::current_render_target_size() const noexcept
    {
        const rhi::rhi_swapchain_t* swapchain{ _rhi->get_swapchain() };

        if (swapchain && swapchain->get_width() > 0 && swapchain->get_height() > 0)
        {
            return { swapchain->get_width(), swapchain->get_height() };
        }

        return { 1u, 1u };
    }

    void renderer_t::build_textured_quad_batches()
    {
        _textured_quad.vertices_cpu.clear();
        _textured_quad.indices_cpu.clear();
        _textured_quad.batches.clear();

        if (_textured_quad.submissions.empty())
            return;

        std::stable_sort(_textured_quad.submissions.begin(), _textured_quad.submissions.end(),
                         [](const textured_quad_state_t::submission_t& lhs,
                            const textured_quad_state_t::submission_t& rhs) noexcept
                         {
                             const auto lhs_layer{ static_cast<uint16_t>(lhs.quad.layer) };
                             const auto rhs_layer{ static_cast<uint16_t>(rhs.quad.layer) };

                             if (lhs_layer != rhs_layer)
                                 return lhs_layer < rhs_layer;

                             if (lhs.quad.order_in_layer != rhs.quad.order_in_layer)
                                 return lhs.quad.order_in_layer < rhs.quad.order_in_layer;

                             return lhs.submission_index < rhs.submission_index;
                         });

        for (const textured_quad_state_t::submission_t& submission: _textured_quad.submissions)
        {
            const textured_quad_draw_info_t& quad{ submission.quad };

            if (_textured_quad.batches.empty() ||
                _textured_quad.batches.back().texture != quad.texture ||
                _textured_quad.batches.back().sampler_preset != quad.sampler_preset)
            {
                _textured_quad.batches.push_back(textured_quad_batch_t{
                    .texture = quad.texture,
                    .first_index = static_cast<uint32_t>(_textured_quad.indices_cpu.size()),
                    .index_count = 0,
                    .sampler_preset = quad.sampler_preset
                });
            }

            const uint32_t base_vertex{ static_cast<uint32_t>(_textured_quad.vertices_cpu.size()) };

            _textured_quad.vertices_cpu.push_back(quad_vertex_t{
                .x = quad.x,
                .y = quad.y,
                .u = quad.u0,
                .v = quad.v0,
                .color = quad.color
            });

            _textured_quad.vertices_cpu.push_back(quad_vertex_t{
                .x = quad.x + quad.width,
                .y = quad.y,
                .u = quad.u1,
                .v = quad.v0,
                .color = quad.color
            });

            _textured_quad.vertices_cpu.push_back(quad_vertex_t{
                .x = quad.x + quad.width,
                .y = quad.y + quad.height,
                .u = quad.u1,
                .v = quad.v1,
                .color = quad.color
            });

            _textured_quad.vertices_cpu.push_back(quad_vertex_t{
                .x = quad.x,
                .y = quad.y + quad.height,
                .u = quad.u0,
                .v = quad.v1,
                .color = quad.color
            });

            _textured_quad.indices_cpu.push_back(base_vertex + 0);
            _textured_quad.indices_cpu.push_back(base_vertex + 1);
            _textured_quad.indices_cpu.push_back(base_vertex + 2);
            _textured_quad.indices_cpu.push_back(base_vertex + 0);
            _textured_quad.indices_cpu.push_back(base_vertex + 2);
            _textured_quad.indices_cpu.push_back(base_vertex + 3);

            _textured_quad.batches.back().index_count += 6;
        }
    }

    void renderer_t::release_frame_resources()
    {
        _textured_quad.submissions.clear();
        _textured_quad.vertices_cpu.clear();
        _textured_quad.indices_cpu.clear();
        _textured_quad.batches.clear();

        _textured_quad.frame_vertex_buffer.reset();
        _textured_quad.frame_index_buffer.reset();

        _textured_quad.vertex_capacity = 0;
        _textured_quad.index_capacity = 0;

        _stats = { };
    }

    void renderer_t::ensure_textured_quad_frame_buffers()
    {
        const size_t required_vertex_bytes{ _textured_quad.vertices_cpu.size() * sizeof(quad_vertex_t) };
        const size_t required_index_bytes{ _textured_quad.indices_cpu.size() * sizeof(uint32_t) };

        if (required_vertex_bytes == 0 || required_index_bytes == 0)
            return;

        const bool needs_vertex_realloc{
            _textured_quad.frame_vertex_buffer != nullptr && _textured_quad.vertex_capacity < required_vertex_bytes
        };
        const bool needs_index_realloc{
            _textured_quad.frame_index_buffer != nullptr && _textured_quad.index_capacity < required_index_bytes
        };

        if ((needs_vertex_realloc || needs_index_realloc) &&
            _rhi->get_graphics_api() == rhi::graphics_api::vulkan)
        {
            // The previous frame may still be using the current geometry buffers.
            // Wait before replacing them so Vulkan backends do not destroy in-flight buffers.
            _rhi->wait_idle();
        }

        if (_textured_quad.frame_vertex_buffer == nullptr || _textured_quad.vertex_capacity < required_vertex_bytes)
        {
            rhi::buffer_create_info_t info{ };
            info.size_bytes = required_vertex_bytes;
            info.usage = rhi::buffer_usage_t::vertex;
            info.initial_data = nullptr;
            info.cpu_writable = true;

            _textured_quad.frame_vertex_buffer = _rhi->create_buffer(info);
            if (!_textured_quad.frame_vertex_buffer)
            {
                LOG_GRAPHICS_FATAL("Failed to create textured quad frame vertex buffer");
                _textured_quad.vertex_capacity = 0;
                return;
            }

            _textured_quad.vertex_capacity = required_vertex_bytes;
        }

        if (_textured_quad.frame_index_buffer == nullptr || _textured_quad.index_capacity < required_index_bytes)
        {
            rhi::buffer_create_info_t info{ };
            info.size_bytes = required_index_bytes;
            info.usage = rhi::buffer_usage_t::index;
            info.initial_data = nullptr;
            info.cpu_writable = true;

            _textured_quad.frame_index_buffer = _rhi->create_buffer(info);
            if (!_textured_quad.frame_index_buffer)
            {
                LOG_GRAPHICS_FATAL("Failed to create textured quad frame index buffer");
                _textured_quad.index_capacity = 0;
                return;
            }

            _textured_quad.index_capacity = required_index_bytes;
        }
    }

    void renderer_t::upload_textured_quad_frame_data() const
    {
        if (_textured_quad.vertices_cpu.empty() || _textured_quad.indices_cpu.empty())
            return;

        if (!_textured_quad.frame_vertex_buffer || !_textured_quad.frame_index_buffer)
        {
            LOG_GRAPHICS_FATAL("Textured quad frame buffers are not available for upload");
            return;
        }

        const size_t vertex_bytes{ _textured_quad.vertices_cpu.size() * sizeof(quad_vertex_t) };
        const size_t index_bytes{ _textured_quad.indices_cpu.size() * sizeof(uint32_t) };

        if (!_textured_quad.frame_vertex_buffer->write(_textured_quad.vertices_cpu.data(), vertex_bytes, 0))
        {
            LOG_GRAPHICS_FATAL("Failed to upload textured quad vertex data");
            return;
        }

        if (!_textured_quad.frame_index_buffer->write(_textured_quad.indices_cpu.data(), index_bytes, 0))
            LOG_GRAPHICS_FATAL("Failed to upload textured quad index data");
    }
} // namespace carrot::renderer
