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
#include "World/World.h"
#include "World/WorldUnits.h"
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
        submit_tilemap(info);
    }

    void renderer_t::submit_tilemap(const tilemap_draw_info_t& info)
    {
        if (!info.tilemap || !info.tilemap->valid())
        {
            LOG_GRAPHICS_WARN("renderer_t::draw_tilemap called with invalid tilemap.");
            return;
        }

        const assets::tilemap_asset_t& tilemap{ info.tilemap->tilemap() };
        const auto& tilesets{ tilemap.tilesets() };
        const auto& textures{ info.tilemap->tileset_textures() };
        const float source_pixels_per_unit{
            info.source_pixels_per_unit > 0.f ? info.source_pixels_per_unit : world::world_units_t::default_pixels_per_unit
        };
        const float render_pixels_per_unit{
            info.render_pixels_per_unit > 0.f ? info.render_pixels_per_unit : source_pixels_per_unit
        };
        const chlm::float2 raw_tile_pixel_size{
            static_cast<float>(tilemap.tile_width()),
            static_cast<float>(tilemap.tile_height())
        };
        const chlm::float2 world_tile_size{
            world::world_units_t::pixel_size_to_world(raw_tile_pixel_size, source_pixels_per_unit)
        };
        const chlm::float2 render_tile_size{
            world::world_units_t::world_size_to_pixels(world_tile_size, render_pixels_per_unit)
        };
        const auto resolve_tileset_index = [&tilesets](const uint32_t gid) -> size_t {
            for (size_t i{ 0 }; i < tilesets.size(); ++i)
            {
                const uint32_t first_gid{ tilesets[i].first_gid };
                const uint32_t next_first_gid{
                    (i + 1u) < tilesets.size() ? tilesets[i + 1u].first_gid : std::numeric_limits<uint32_t>::max()
                };

                if (gid >= first_gid && gid < next_first_gid)
                    return i;
            }

            return static_cast<size_t>(-1);
        };
        const auto populate_tile_uvs = [&tilesets](const size_t tileset_index,
                                                   const uint32_t gid,
                                                   textured_quad_draw_info_t& quad) -> bool {
            if (tileset_index == static_cast<size_t>(-1))
                return false;

            const assets::tilemap_tileset_t& tileset{ tilesets[tileset_index] };
            if (tileset.columns == 0 || tileset.tile_width == 0 || tileset.tile_height == 0 ||
                tileset.image_width == 0 || tileset.image_height == 0)
            {
                return false;
            }

            const uint32_t local_tile_index{ gid - tileset.first_gid };
            const uint32_t tile_u_index{ local_tile_index % tileset.columns };
            const uint32_t tile_v_index{ local_tile_index / tileset.columns };

            quad.u0 = static_cast<float>(tile_u_index * tileset.tile_width) /
                      static_cast<float>(tileset.image_width);
            quad.v0 = static_cast<float>(tile_v_index * tileset.tile_height) /
                      static_cast<float>(tileset.image_height);
            quad.u1 = static_cast<float>((tile_u_index + 1u) * tileset.tile_width) /
                      static_cast<float>(tileset.image_width);
            quad.v1 = static_cast<float>((tile_v_index + 1u) * tileset.tile_height) /
                      static_cast<float>(tileset.image_height);

            return true;
        };

        for (const assets::tilemap_layer_t& layer : tilemap.layers())
        {
            if (!layer.visible)
                continue;

            if (layer.kind == assets::tilemap_layer_kind_t::tile)
            {
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

                        const size_t tileset_index{ resolve_tileset_index(gid) };
                        if (tileset_index == static_cast<size_t>(-1) ||
                            tileset_index >= textures.size() ||
                            !textures[tileset_index])
                        {
                            continue;
                        }

                        textured_quad_draw_info_t tile_quad{ };
                        tile_quad.texture = textures[tileset_index].get();
                        tile_quad.x = info.origin.x + (static_cast<float>(col) * render_tile_size.x * info.scale.x);
                        tile_quad.y = info.origin.y + (static_cast<float>(row) * render_tile_size.y * info.scale.y);
                        tile_quad.width = render_tile_size.x * info.scale.x;
                        tile_quad.height = render_tile_size.y * info.scale.y;
                        tile_quad.layer = info.layer;
                        tile_quad.order_in_layer = info.order_in_layer;
                        tile_quad.color = info.color;
                        tile_quad.sampler_preset = info.sampler_preset;

                        if (!populate_tile_uvs(tileset_index, gid, tile_quad))
                            continue;

                        draw_textured_quad(tile_quad);
                    }
                }
            }

            if (layer.kind != assets::tilemap_layer_kind_t::object || !info.include_object_layers)
                continue;

            int32_t object_order{ info.order_in_layer };
            for (const assets::tilemap_object_t& object : layer.objects)
            {
                if (!object.visible || object.gid == 0)
                    continue;

                const size_t tileset_index{ resolve_tileset_index(object.gid) };
                if (tileset_index == static_cast<size_t>(-1) ||
                    tileset_index >= textures.size() ||
                    !textures[tileset_index])
                {
                    continue;
                }

                textured_quad_draw_info_t object_quad{ };
                object_quad.texture = textures[tileset_index].get();
                const chlm::float2 raw_object_pixel_size{ object.width, object.height };
                const chlm::float2 object_world_size{
                    world::world_units_t::pixel_size_to_world(raw_object_pixel_size, source_pixels_per_unit)
                };
                const chlm::float2 object_render_size{
                    world::world_units_t::world_size_to_pixels(object_world_size, render_pixels_per_unit)
                };
                const float object_x_offset{
                    world::world_units_t::world_to_pixels(
                        world::world_units_t::pixels_to_world(object.x, source_pixels_per_unit),
                        render_pixels_per_unit)
                };
                const float object_y_offset{
                    world::world_units_t::world_to_pixels(
                        world::world_units_t::pixels_to_world(object.y, source_pixels_per_unit),
                        render_pixels_per_unit)
                };
                object_quad.x = info.origin.x + (object_x_offset * info.scale.x);
                object_quad.y = info.origin.y + ((object_y_offset - object_render_size.y) * info.scale.y);
                object_quad.width = object_render_size.x * info.scale.x;
                object_quad.height = object_render_size.y * info.scale.y;
                object_quad.layer = info.layer;
                object_quad.order_in_layer = object_order++;
                object_quad.color = info.color;
                object_quad.sampler_preset = info.sampler_preset;

                if (!populate_tile_uvs(tileset_index, object.gid, object_quad))
                    continue;

                draw_textured_quad(object_quad);
            }
        }
    }

    void renderer_t::submit_tile_object(const assets::loaded_tilemap_asset_t& tilemap,
                                        const uint32_t gid,
                                        const chlm::float2& position_px,
                                        const chlm::float2& size_px,
                                        const render_layer_t layer,
                                        const int32_t order_in_layer,
                                        const quad_sampler_preset_t sampler_preset,
                                        const uint32_t color)
    {
        if (!tilemap.valid() || gid == 0)
            return;

        const assets::tilemap_asset_t& tilemap_asset{ tilemap.tilemap() };
        const auto& tilesets{ tilemap_asset.tilesets() };
        const auto& textures{ tilemap.tileset_textures() };
        const auto resolve_tileset_index = [&tilesets](const uint32_t object_gid) -> size_t {
            for (size_t i{ 0 }; i < tilesets.size(); ++i)
            {
                const uint32_t first_gid{ tilesets[i].first_gid };
                const uint32_t next_first_gid{
                    (i + 1u) < tilesets.size() ? tilesets[i + 1u].first_gid : std::numeric_limits<uint32_t>::max()
                };

                if (object_gid >= first_gid && object_gid < next_first_gid)
                    return i;
            }

            return static_cast<size_t>(-1);
        };

        const size_t tileset_index{ resolve_tileset_index(gid) };
        if (tileset_index == static_cast<size_t>(-1) ||
            tileset_index >= textures.size() ||
            !textures[tileset_index])
        {
            return;
        }

        const assets::tilemap_tileset_t& tileset{ tilesets[tileset_index] };
        if (tileset.columns == 0 || tileset.tile_width == 0 || tileset.tile_height == 0 ||
            tileset.image_width == 0 || tileset.image_height == 0)
        {
            return;
        }

        const uint32_t local_tile_index{ gid - tileset.first_gid };
        const uint32_t tile_u_index{ local_tile_index % tileset.columns };
        const uint32_t tile_v_index{ local_tile_index / tileset.columns };

        textured_quad_draw_info_t quad{ };
        quad.texture = textures[tileset_index].get();
        quad.x = position_px.x;
        quad.y = position_px.y;
        quad.width = size_px.x;
        quad.height = size_px.y;
        quad.layer = layer;
        quad.order_in_layer = order_in_layer;
        quad.color = color;
        quad.sampler_preset = sampler_preset;
        quad.u0 = static_cast<float>(tile_u_index * tileset.tile_width) / static_cast<float>(tileset.image_width);
        quad.v0 = static_cast<float>(tile_v_index * tileset.tile_height) / static_cast<float>(tileset.image_height);
        quad.u1 = static_cast<float>((tile_u_index + 1u) * tileset.tile_width) / static_cast<float>(tileset.image_width);
        quad.v1 = static_cast<float>((tile_v_index + 1u) * tileset.tile_height) / static_cast<float>(tileset.image_height);
        draw_textured_quad(quad);
    }

    void renderer_t::draw_world(const world::world_t& world)
    {
        const chlm::float2 render_origin_px{ world.render_origin_px() };
        const float render_pixels_per_unit{ world.render_pixels_per_unit() };

        for (const world::world_object_t& object : world.objects())
        {
            if (!object.transform || (!object.sprite && !object.tilemap && !object.tile_object))
                continue;

            const world::transform_component_t& transform{ *object.transform };
            const chlm::float2 render_position_px{
                render_origin_px + world::world_units_t::world_size_to_pixels(transform.position, render_pixels_per_unit)
            };

            if (object.tilemap)
            {
                const world::tilemap_component_t& tilemap{ *object.tilemap };
                submit_tilemap({
                    .tilemap = tilemap.tilemap,
                    .origin = render_position_px,
                    .scale = transform.scale,
                    .source_pixels_per_unit = world::world_units_t::default_pixels_per_unit,
                    .render_pixels_per_unit = render_pixels_per_unit,
                    .include_object_layers = tilemap.include_object_layers,
                    .layer = tilemap.layer,
                    .order_in_layer = tilemap.order_in_layer,
                    .sampler_preset = tilemap.sampler_preset,
                    .color = tilemap.color
                });
            }

            if (object.tile_object)
            {
                const world::tile_object_component_t& tile_object{ *object.tile_object };
                if (!tile_object.tilemap || tile_object.gid == 0)
                    continue;

                const chlm::float2 world_size{
                    world::world_units_t::pixel_size_to_world(tile_object.size_source_px,
                                                              world::world_units_t::default_pixels_per_unit)
                };
                const chlm::float2 render_size_px{
                    world::world_units_t::world_size_to_pixels(world_size, render_pixels_per_unit)
                };

                submit_tile_object(*tile_object.tilemap,
                                   tile_object.gid,
                                   render_position_px,
                                   { render_size_px.x * transform.scale.x, render_size_px.y * transform.scale.y },
                                   tile_object.layer,
                                   tile_object.order_in_layer,
                                   tile_object.sampler_preset,
                                   tile_object.color);
            }

            if (object.sprite)
            {
                const world::sprite_component_t& sprite{ *object.sprite };
                const assets::sprite_frame_t* frame{
                    object.sprite_animator ? object.sprite_animator->animator.current_frame() : sprite.frame
                };

                if (!sprite.sprite || !frame)
                    continue;

                const chlm::float2 pixel_size{
                    static_cast<float>(frame->pixel_rect.size.x),
                    static_cast<float>(frame->pixel_rect.size.y)
                };
                const float sprite_pixels_per_unit{
                    sprite.sprite->sprite().pixels_per_unit() > 0.f
                        ? sprite.sprite->sprite().pixels_per_unit()
                        : world::world_units_t::default_pixels_per_unit
                };
                const chlm::float2 native_world_size{
                    world::world_units_t::pixel_size_to_world(pixel_size, sprite_pixels_per_unit)
                };
                const chlm::float2 final_world_size{
                    sprite.use_size_override ? sprite.size_override_world : native_world_size
                };
                const chlm::float2 render_size_px{
                    world::world_units_t::world_size_to_pixels(final_world_size, render_pixels_per_unit)
                };

                sprite_draw_info_t draw_info{ };
                draw_info.sprite = sprite.sprite;
                draw_info.frame = frame;
                draw_info.x = render_position_px.x;
                draw_info.y = render_position_px.y;
                draw_info.width = render_size_px.x * transform.scale.x;
                draw_info.height = render_size_px.y * transform.scale.y;
                draw_info.use_custom_pivot = sprite.use_custom_pivot;
                draw_info.pivot = sprite.pivot;
                draw_info.flip_x = sprite.flip_x;
                draw_info.flip_y = sprite.flip_y;
                draw_info.layer = sprite.layer;
                draw_info.order_in_layer = sprite.order_in_layer;
                draw_info.color = sprite.color;
                draw_info.sampler_preset = sprite.sampler_preset;

                draw_sprite(draw_info);
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
