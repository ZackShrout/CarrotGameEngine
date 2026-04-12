//
// Created by zshrout on 1/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Renderer.h"

#include "Assets/Sprite/LoadedSpriteAsset.h"
#include "Assets/Texture/TextureAsset.h"
#include "Assets/Tilemap/LoadedTilemapAsset.h"
#include "Draw/SpriteDrawInfo.h"
#include "IO/VirtualFileSystem.h"
#include "RHI/Buffer.h"
#include "RHI/Swapchain.h"
#include "Window/Window.h"
#include "World/World.h"
#include "World/WorldLayering.h"
#include "World/WorldUnits.h"

namespace carrot::renderer {
    namespace {
        [[nodiscard]] constexpr size_t frame_stage_index(const frame_stage_kind_t stage) noexcept
        {
            return static_cast<size_t>(stage);
        }

        [[nodiscard]] bool quad_sorts_before(const textured_quad_draw_info_t& lhs,
                                             const textured_quad_draw_info_t& rhs) noexcept
        {
            const auto lhs_layer{ static_cast<uint16_t>(lhs.layer) };
            const auto rhs_layer{ static_cast<uint16_t>(rhs.layer) };

            if (lhs_layer != rhs_layer)
                return lhs_layer < rhs_layer;

            const auto lhs_order_mode{ static_cast<uint8_t>(lhs.order_mode) };
            const auto rhs_order_mode{ static_cast<uint8_t>(rhs.order_mode) };
            if (lhs_order_mode != rhs_order_mode)
                return lhs_order_mode < rhs_order_mode;

            if (lhs.order_mode == render_order_mode_t::anchor_bottom_y)
            {
                if (lhs.sort_reference_y < rhs.sort_reference_y)
                    return true;
                if (rhs.sort_reference_y < lhs.sort_reference_y)
                    return false;
            }

            if (lhs.order_in_layer != rhs.order_in_layer)
                return lhs.order_in_layer < rhs.order_in_layer;

            return false;
        }

        [[nodiscard]] const world::world_object_t* resolve_primary_visibility_anchor(const world::world_t& world) noexcept
        {
            for (const world::world_object_t& object : world.objects())
            {
                if (object.type == "Character" && object.transform)
                    return &object;
            }

            for (const world::world_object_t& object : world.objects())
            {
                if (object.sprite && object.transform)
                    return &object;
            }

            return nullptr;
        }

        void append_layering_debug_entry(world::layering_debug_snapshot_t& snapshot,
                                         const assets::tilemap_layer_t& layer,
                                         const world::authored_layer_semantics_t& semantics,
                                         const bool is_visible) noexcept
        {
            const bool is_conditional_front{ layer.get_bool_property("carrot_conditional_front").value_or(false) };
            const bool is_always_front{ layer.get_bool_property("carrot_always_front").value_or(false) };
            const bool has_visibility_zone{ !semantics.visibility_tag.empty() };

            snapshot.layer_count++;
            if (is_visible)
                snapshot.visible_layer_count++;
            else
                snapshot.hidden_layer_count++;

            if (has_visibility_zone)
                snapshot.visibility_bound_layer_count++;

            if (is_conditional_front)
                snapshot.conditional_front_layer_count++;

            if (is_always_front)
                snapshot.always_front_layer_count++;

            snapshot.layers.push_back({
                .layer_name = layer.name,
                .source_kind = layer.kind == assets::tilemap_layer_kind_t::object ? "object" : "tile",
                .resolved_render_layer = semantics.render_layer,
                .resolved_order_mode = semantics.order_mode,
                .resolved_order_in_layer = semantics.order_in_layer,
                .visibility_zone_id = semantics.visibility_tag,
                .is_visible = is_visible,
                .hidden_by_visibility_zone = has_visibility_zone && !is_visible,
                .is_conditional_front = is_conditional_front,
                .is_always_front = is_always_front
            });
        }
    } // namespace

    // PUBLIC

    renderer_t::renderer_t(io::virtual_file_system_t& vfs,
                           const engine_graphics_config_t& config,
                           const window::window_id_t render_window_id)
        : _vfs{ vfs }, _config{ config }, _render_window_id{ render_window_id }
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
        desc.presentation_window_id = window::has_window(_render_window_id)
                                          ? _render_window_id
                                          : window::get_main_window_id();
        desc.width = window::get_width(desc.presentation_window_id);
        desc.height = window::get_height(desc.presentation_window_id);
        desc.shader_files = _shader_provider.get();

        _rhi = rhi::create_rhi_context(desc);
        if (!_rhi)
        {
            LOG_GRAPHICS_FATAL("Failed to create RHI context!");
            return;
        }

        constexpr std::array<uint8_t, 4> white_pixel{ 0xFFu, 0xFFu, 0xFFu, 0xFFu };
        rhi::texture_create_info_t white_texture_info{ };
        white_texture_info.width = 1;
        white_texture_info.height = 1;
        white_texture_info.format = rhi::texture_format_t::rgba8_unorm;
        white_texture_info.initial_data = white_pixel.data();
        white_texture_info.initial_data_size = white_pixel.size();
        white_texture_info.initial_data_stride_bytes = 4;
        _solid_white_texture = _rhi->create_texture_2d(white_texture_info);
        if (!_solid_white_texture)
        {
            LOG_GRAPHICS_FATAL("Failed to create renderer solid white texture");
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

        _solid_white_texture.reset();
        _shader_provider.reset();
        _rhi.reset();

        _is_initialized = false;
        LOG_GRAPHICS_INFO("Renderer shutdown complete");
    }

    void renderer_t::begin_frame()
    {
        _frame_index++;
        _animated_tiles_elapsed_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - _animated_tiles_clock_origin).count());

        for (textured_quad_state_t& stage_state : _stage_textured_quads)
        {
            stage_state.submissions.clear();
            stage_state.vertices_cpu.clear();
            stage_state.indices_cpu.clear();
            stage_state.batches.clear();
        }
        for (textured_quad_state_t& stage_state : _stage_text_quads)
        {
            stage_state.submissions.clear();
            stage_state.vertices_cpu.clear();
            stage_state.indices_cpu.clear();
            stage_state.batches.clear();
        }

        _stats = { };
        _fullscreen_overlay_enabled = false;
        _fullscreen_overlay_color = 0x00000000u;

        _rhi->begin_frame();
    }

    void renderer_t::end_frame()
    {
        queue_fullscreen_overlay_if_needed();
        execute_frame_stages();
        _last_completed_stats = _stats;

        _rhi->end_frame();
    }

    bool renderer_t::add_presentation_window(const window::window_id_t window_id,
                                             const uint32_t presentation_channel_mask)
    {
        if (!_rhi || window_id == window::invalid_window_id || window_id == _render_window_id)
            return false;

        return _rhi->add_presentation_window(window_id, presentation_channel_mask);
    }

    bool renderer_t::remove_presentation_window(const window::window_id_t window_id)
    {
        if (!_rhi || window_id == window::invalid_window_id)
            return false;

        return _rhi->remove_presentation_window(window_id);
    }

    void renderer_t::draw_textured_quad(const textured_quad_draw_info_t& quad)
    {
        submit_textured_quad(frame_stage_kind_t::world, quad);
    }

    void renderer_t::draw_overlay_textured_quad(const textured_quad_draw_info_t& quad)
    {
        submit_textured_quad(frame_stage_kind_t::overlay_debug, quad);
    }

    void renderer_t::draw_ui_textured_quad(const textured_quad_draw_info_t& quad)
    {
        submit_textured_quad(frame_stage_kind_t::ui, quad);
    }

    void renderer_t::draw_log_console_textured_quad(const textured_quad_draw_info_t& quad)
    {
        submit_textured_quad(frame_stage_kind_t::log_console, quad);
    }

    void renderer_t::draw_text_quad(const textured_quad_draw_info_t& quad)
    {
        submit_text_quad(frame_stage_kind_t::world, quad);
    }

    void renderer_t::draw_overlay_text_quad(const textured_quad_draw_info_t& quad)
    {
        submit_text_quad(frame_stage_kind_t::overlay_debug, quad);
    }

    void renderer_t::draw_ui_text_quad(const textured_quad_draw_info_t& quad)
    {
        submit_text_quad(frame_stage_kind_t::ui, quad);
    }

    void renderer_t::draw_log_console_text_quad(const textured_quad_draw_info_t& quad)
    {
        submit_text_quad(frame_stage_kind_t::log_console, quad);
    }

    void renderer_t::draw_solid_quad(const solid_quad_draw_info_t& quad)
    {
        submit_solid_quad(frame_stage_kind_t::world, quad);
    }

    void renderer_t::draw_overlay_solid_quad(const solid_quad_draw_info_t& quad)
    {
        submit_solid_quad(frame_stage_kind_t::overlay_debug, quad);
    }

    void renderer_t::draw_ui_solid_quad(const solid_quad_draw_info_t& quad)
    {
        submit_solid_quad(frame_stage_kind_t::ui, quad);
    }

    void renderer_t::draw_log_console_solid_quad(const solid_quad_draw_info_t& quad)
    {
        submit_solid_quad(frame_stage_kind_t::log_console, quad);
    }

    void renderer_t::set_fullscreen_overlay_color(const uint32_t color_abgr) noexcept
    {
        _fullscreen_overlay_enabled = true;
        _fullscreen_overlay_color = color_abgr;
    }

    void renderer_t::clear_fullscreen_overlay() noexcept
    {
        _fullscreen_overlay_enabled = false;
        _fullscreen_overlay_color = 0x00000000u;
    }

    void renderer_t::submit_textured_quad(const frame_stage_kind_t stage, const textured_quad_draw_info_t& quad)
    {
        if (quad.texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_textured_quad called with null texture");
            return;
        }

        textured_quad_state_t& stage_state{ _stage_textured_quads[frame_stage_index(stage)] };
        stage_state.submissions.push_back({
            .quad = quad,
            .submission_index = static_cast<uint64_t>(stage_state.submissions.size())
        });

        _stats.textured_quad_count++;
    }

    void renderer_t::submit_solid_quad(const frame_stage_kind_t stage, const solid_quad_draw_info_t& quad)
    {
        if (_solid_white_texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_solid_quad called before renderer solid white texture was ready");
            return;
        }

        submit_textured_quad(stage, textured_quad_draw_info_t{
            .texture = _solid_white_texture.get(),
            .x = quad.x,
            .y = quad.y,
            .width = quad.width,
            .height = quad.height,
            .u0 = 0.f,
            .v0 = 0.f,
            .u1 = 1.f,
            .v1 = 1.f,
            .layer = quad.layer,
            .order_mode = quad.order_mode,
            .order_in_layer = quad.order_in_layer,
            .sort_reference_y = quad.sort_reference_y,
            .color = quad.color,
            .sampler_preset = quad.sampler_preset
        });
    }

    void renderer_t::submit_text_quad(const frame_stage_kind_t stage, const textured_quad_draw_info_t& quad)
    {
        if (quad.texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_text_quad called with null texture");
            return;
        }

        textured_quad_state_t& stage_state{ _stage_text_quads[frame_stage_index(stage)] };
        stage_state.submissions.push_back({
            .quad = quad,
            .submission_index = static_cast<uint64_t>(stage_state.submissions.size())
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
        quad.order_mode = info.order_mode;
        quad.order_in_layer = info.order_in_layer;
        quad.sort_reference_y = info.order_mode == render_order_mode_t::anchor_bottom_y
                                    ? (quad.y + quad.height)
                                    : info.sort_reference_y;
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

    void renderer_t::submit_tilemap(const tilemap_draw_info_t& info, world::layering_debug_snapshot_t* layering_debug_snapshot)
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

        if (layering_debug_snapshot)
            layering_debug_snapshot->rendered_tilemap_count++;

        const auto& layers{ tilemap.layers() };
        for (size_t layer_index{ 0 }; layer_index < layers.size(); ++layer_index)
        {
            const assets::tilemap_layer_t& layer{ layers[layer_index] };
            if (!layer.visible)
                continue;

            if (layer.kind == assets::tilemap_layer_kind_t::tile)
            {
                const world::authored_layer_semantics_t layer_semantics{
                    world::resolve_tile_layer_semantics(layer, static_cast<int32_t>(layer_index))
                };
                const bool is_layer_visible{ world::is_layer_visible(layer_semantics, info.active_visibility_tags) };
                if (layering_debug_snapshot)
                    append_layering_debug_entry(*layering_debug_snapshot, layer, layer_semantics, is_layer_visible);

                if (!is_layer_visible)
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

                        const size_t tileset_index{ resolve_tileset_index(gid) };
                        if (tileset_index == static_cast<size_t>(-1) ||
                            tileset_index >= textures.size() ||
                            !textures[tileset_index])
                        {
                            continue;
                        }

                        const assets::tilemap_tileset_t& tileset{ tilesets[tileset_index] };
                        const uint32_t local_tile_id{ gid - tileset.first_gid };
                        const uint32_t resolved_local_tile_id{
                            tileset.resolve_animated_tile_id(local_tile_id, _animated_tiles_elapsed_ms)
                        };
                        const uint32_t resolved_gid{ tileset.first_gid + resolved_local_tile_id };

                        textured_quad_draw_info_t tile_quad{ };
                        tile_quad.texture = textures[tileset_index].get();
                        tile_quad.x = info.origin.x + (static_cast<float>(col) * render_tile_size.x * info.scale.x);
                        tile_quad.y = info.origin.y + (static_cast<float>(row) * render_tile_size.y * info.scale.y);
                        tile_quad.width = render_tile_size.x * info.scale.x;
                        tile_quad.height = render_tile_size.y * info.scale.y;
                        tile_quad.layer = layer_semantics.render_layer;
                        tile_quad.order_mode = layer_semantics.order_mode;
                        tile_quad.order_in_layer = layer_semantics.order_in_layer;
                        tile_quad.sort_reference_y = layer_semantics.order_mode == render_order_mode_t::anchor_bottom_y
                                                         ? (tile_quad.y + tile_quad.height)
                                                         : info.sort_reference_y;
                        tile_quad.color = info.color;
                        tile_quad.sampler_preset = info.sampler_preset;

                        if (!populate_tile_uvs(tileset_index, resolved_gid, tile_quad))
                            continue;

                        draw_textured_quad(tile_quad);
                    }
                }
            }

            if (layer.kind != assets::tilemap_layer_kind_t::object || !info.include_object_layers)
                continue;

            const world::authored_layer_semantics_t layer_semantics{
                world::resolve_object_layer_semantics(layer, static_cast<int32_t>(layer_index))
            };
            const bool is_layer_visible{ world::is_layer_visible(layer_semantics, info.active_visibility_tags) };
            if (layering_debug_snapshot)
                append_layering_debug_entry(*layering_debug_snapshot, layer, layer_semantics, is_layer_visible);

            if (!is_layer_visible)
                continue;

            int32_t object_order{ layer_semantics.order_in_layer };
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

                const assets::tilemap_tileset_t& tileset{ tilesets[tileset_index] };
                const uint32_t local_tile_id{ object.gid - tileset.first_gid };
                const uint32_t resolved_local_tile_id{
                    tileset.resolve_animated_tile_id(local_tile_id, _animated_tiles_elapsed_ms)
                };
                const uint32_t resolved_gid{ tileset.first_gid + resolved_local_tile_id };

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
                object_quad.layer = layer_semantics.render_layer;
                object_quad.order_mode = layer_semantics.order_mode;
                object_quad.order_in_layer = object_order++;
                object_quad.sort_reference_y = layer_semantics.order_mode == render_order_mode_t::anchor_bottom_y
                                                   ? (object_quad.y + object_quad.height)
                                                   : info.sort_reference_y;
                object_quad.color = info.color;
                object_quad.sampler_preset = info.sampler_preset;

                if (!populate_tile_uvs(tileset_index, resolved_gid, object_quad))
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
                                        const render_order_mode_t order_mode,
                                        const int32_t order_in_layer,
                                        const float sort_reference_y,
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

        const uint32_t resolved_local_tile_id{
            tileset.resolve_animated_tile_id(gid - tileset.first_gid, _animated_tiles_elapsed_ms)
        };
        const uint32_t local_tile_index{ resolved_local_tile_id };
        const uint32_t tile_u_index{ local_tile_index % tileset.columns };
        const uint32_t tile_v_index{ local_tile_index / tileset.columns };

        textured_quad_draw_info_t quad{ };
        quad.texture = textures[tileset_index].get();
        quad.x = position_px.x;
        quad.y = position_px.y;
        quad.width = size_px.x;
        quad.height = size_px.y;
        quad.layer = layer;
        quad.order_mode = order_mode;
        quad.order_in_layer = order_in_layer;
        quad.sort_reference_y = order_mode == render_order_mode_t::anchor_bottom_y
                                    ? (quad.y + quad.height)
                                    : sort_reference_y;
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
        const world::world_presentation_t& presentation{ world.presentation() };
        std::vector<std::string_view> active_visibility_tags;
        world::layering_debug_snapshot_t layering_debug_snapshot{ };
        layering_debug_snapshot.frame_index = _frame_index;
        if (const world::world_object_t* visibility_anchor{ resolve_primary_visibility_anchor(world) };
            visibility_anchor && visibility_anchor->transform)
        {
            layering_debug_snapshot.has_visibility_anchor = true;
            layering_debug_snapshot.visibility_anchor_world = visibility_anchor->transform->position;
            active_visibility_tags = world.collect_active_visibility_tags(visibility_anchor->transform->position);
        }
        layering_debug_snapshot.active_visibility_tags.reserve(active_visibility_tags.size());
        for (const std::string_view tag : active_visibility_tags)
            layering_debug_snapshot.active_visibility_tags.emplace_back(tag);

        for (const world::world_object_t& object : world.objects())
        {
            if (object.visibility_region)
                layering_debug_snapshot.visibility_region_count++;
        }

        for (const world::world_object_t& object : world.objects())
        {
            if (!object.transform || (!object.sprite && !object.tilemap && !object.tile_object))
                continue;

            const world::transform_component_t& transform{ *object.transform };
            const chlm::float2 render_position_px{
                presentation.world_position_to_pixels(transform.position)
            };

            if (object.tilemap)
            {
                const world::tilemap_component_t& tilemap{ *object.tilemap };
                submit_tilemap({
                    .tilemap = tilemap.tilemap,
                    .origin = render_position_px,
                    .scale = transform.scale,
                    .source_pixels_per_unit = world::world_units_t::default_pixels_per_unit,
                    .render_pixels_per_unit = presentation.pixels_per_unit,
                    .include_object_layers = tilemap.include_object_layers,
                    .active_visibility_tags = active_visibility_tags,
                    .layer = tilemap.layer,
                    .order_mode = tilemap.order_mode,
                    .order_in_layer = tilemap.order_in_layer,
                    .sort_reference_y = tilemap.sort_reference_y,
                    .sampler_preset = tilemap.sampler_preset,
                    .color = tilemap.color
                }, &layering_debug_snapshot);
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
                const chlm::float2 render_size_px{ presentation.world_size_to_pixels(world_size) };

                submit_tile_object(*tile_object.tilemap,
                                   tile_object.gid,
                                   render_position_px,
                                   { render_size_px.x * transform.scale.x, render_size_px.y * transform.scale.y },
                                   tile_object.layer,
                                   tile_object.order_mode,
                                   tile_object.order_in_layer,
                                   tile_object.sort_reference_y,
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
                const chlm::float2 render_size_px{ presentation.world_size_to_pixels(final_world_size) };

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
                draw_info.order_mode = sprite.order_mode;
                draw_info.order_in_layer = sprite.order_in_layer;
                draw_info.sort_reference_y = sprite.sort_reference_y;
                draw_info.color = sprite.color;
                draw_info.sampler_preset = sprite.sampler_preset;

                draw_sprite(draw_info);
            }

            if (object.visibility_region && object.transform && world.layering_debug_view().show_visibility_regions)
            {
                const chlm::float2 render_size_px{
                    presentation.world_size_to_pixels(object.visibility_region->size_world)
                };
                draw_overlay_solid_quad({
                    .x = render_position_px.x,
                    .y = render_position_px.y,
                    .width = render_size_px.x,
                    .height = render_size_px.y,
                    .layer = render_layer_t::debug,
                    .order_mode = render_order_mode_t::explicit_order,
                    .order_in_layer = 0,
                    .color = world.layering_debug_view().visibility_region_color,
                    .sampler_preset = quad_sampler_preset_t::pixel_clamp
                });
            }
        }

        world.set_layering_debug_snapshot(std::move(layering_debug_snapshot));
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

    renderer_t::stage_execution_context_t renderer_t::resolve_stage_execution_context(
        const frame_stage_plan_t& stage_plan) const noexcept
    {
        const chlm::uint2 render_target_size{ current_render_target_size() };
        const resolved_camera_2d_t resolved_world_camera{ _active_camera.resolve(render_target_size) };

        switch (stage_plan.space)
        {
            case frame_stage_space_t::viewport_pixels:
            {
                const float viewport_width{
                    static_cast<float>(std::max(1u, resolved_world_camera.viewport_rect_px.size.x))
                };
                const float viewport_height{
                    static_cast<float>(std::max(1u, resolved_world_camera.viewport_rect_px.size.y))
                };

                return {
                    .view_projection = chlm::float4x4::ortho_off_center_lh_top_left(
                        0.f, viewport_width, 0.f, viewport_height, 0.f, 1.f
                    ),
                    .viewport = {
                        .rect_px = resolved_world_camera.viewport_rect_px
                    }
                };
            }

            case frame_stage_space_t::render_target_pixels:
            {
                const float render_width{ static_cast<float>(std::max(1u, render_target_size.x)) };
                const float render_height{ static_cast<float>(std::max(1u, render_target_size.y)) };

                return {
                    .view_projection = chlm::float4x4::ortho_off_center_lh_top_left(
                        0.f, render_width, 0.f, render_height, 0.f, 1.f
                    ),
                    .viewport = {
                        .rect_px = {
                            .position = { 0u, 0u },
                            .size = { std::max(1u, render_target_size.x), std::max(1u, render_target_size.y) }
                        }
                    }
                };
            }

            case frame_stage_space_t::world_camera:
            default:
            {
                return {
                    .view_projection = _active_camera.view_projection_matrix(render_target_size),
                    .viewport = {
                        .rect_px = resolved_world_camera.viewport_rect_px
                    }
                };
            }
        }
    }

    void renderer_t::queue_fullscreen_overlay_if_needed()
    {
        if (!_fullscreen_overlay_enabled || _solid_white_texture == nullptr)
            return;

        const chlm::uint2 render_target_size{ current_render_target_size() };

        submit_textured_quad(frame_stage_kind_t::composite, textured_quad_draw_info_t{
            .texture = _solid_white_texture.get(),
            .x = 0.f,
            .y = 0.f,
            .width = static_cast<float>(std::max(1u, render_target_size.x)),
            .height = static_cast<float>(std::max(1u, render_target_size.y)),
            .u0 = 0.f,
            .v0 = 0.f,
            .u1 = 1.f,
            .v1 = 1.f,
            .layer = render_layer_t::ui,
            .order_in_layer = 0,
            .color = _fullscreen_overlay_color,
            .sampler_preset = quad_sampler_preset_t::pixel_clamp
        });
    }

    void renderer_t::build_textured_quad_batches(textured_quad_state_t& state) const
    {
        state.vertices_cpu.clear();
        state.indices_cpu.clear();
        state.batches.clear();

        if (state.submissions.empty())
            return;

        std::stable_sort(state.submissions.begin(), state.submissions.end(),
                         [](const textured_quad_state_t::submission_t& lhs,
                            const textured_quad_state_t::submission_t& rhs) noexcept
                         {
                             if (quad_sorts_before(lhs.quad, rhs.quad))
                                 return true;

                             if (quad_sorts_before(rhs.quad, lhs.quad))
                                 return false;

                             return lhs.submission_index < rhs.submission_index;
                         });

        for (const textured_quad_state_t::submission_t& submission: state.submissions)
        {
            const textured_quad_draw_info_t& quad{ submission.quad };

            if (state.batches.empty() ||
                state.batches.back().texture != quad.texture ||
                state.batches.back().sampler_preset != quad.sampler_preset)
            {
                state.batches.push_back(textured_quad_batch_t{
                    .texture = quad.texture,
                    .first_index = static_cast<uint32_t>(state.indices_cpu.size()),
                    .index_count = 0,
                    .sampler_preset = quad.sampler_preset
                });
            }

            const uint32_t base_vertex{ static_cast<uint32_t>(state.vertices_cpu.size()) };

            state.vertices_cpu.push_back(quad_vertex_t{
                .x = quad.x,
                .y = quad.y,
                .u = quad.u0,
                .v = quad.v0,
                .color = quad.color,
                .effect_mode = quad.effect_mode,
                .effect_param0 = quad.effect_param0
            });

            state.vertices_cpu.push_back(quad_vertex_t{
                .x = quad.x + quad.width,
                .y = quad.y,
                .u = quad.u1,
                .v = quad.v0,
                .color = quad.color,
                .effect_mode = quad.effect_mode,
                .effect_param0 = quad.effect_param0
            });

            state.vertices_cpu.push_back(quad_vertex_t{
                .x = quad.x + quad.width,
                .y = quad.y + quad.height,
                .u = quad.u1,
                .v = quad.v1,
                .color = quad.color,
                .effect_mode = quad.effect_mode,
                .effect_param0 = quad.effect_param0
            });

            state.vertices_cpu.push_back(quad_vertex_t{
                .x = quad.x,
                .y = quad.y + quad.height,
                .u = quad.u0,
                .v = quad.v1,
                .color = quad.color,
                .effect_mode = quad.effect_mode,
                .effect_param0 = quad.effect_param0
            });

            state.indices_cpu.push_back(base_vertex + 0);
            state.indices_cpu.push_back(base_vertex + 1);
            state.indices_cpu.push_back(base_vertex + 2);
            state.indices_cpu.push_back(base_vertex + 0);
            state.indices_cpu.push_back(base_vertex + 2);
            state.indices_cpu.push_back(base_vertex + 3);

            state.batches.back().index_count += 6;
        }
    }

    void renderer_t::execute_frame_stage(const frame_stage_plan_t& stage_plan)
    {
        const stage_execution_context_t stage_context{ resolve_stage_execution_context(stage_plan) };
        const uint32_t presentation_mask{ stage_plan.kind == frame_stage_kind_t::log_console
                                              ? rhi::presentation_channel_log_console
                                              : rhi::presentation_channel_gameplay };

        auto record_stage = [&](textured_quad_state_t& stage_state, const bool is_text)
        {
            build_textured_quad_batches(stage_state);

            if (stage_state.batches.empty())
                return;

            ensure_textured_quad_frame_buffers(stage_state);
            upload_textured_quad_frame_data(stage_state);

            const auto& frame_buffers{ current_frame_buffers(stage_state) };
            if (!frame_buffers.vertex_buffer || !frame_buffers.index_buffer)
                return;

            const rhi::textured_quad_stage_record_t record{
                .vertex_buffer = frame_buffers.vertex_buffer.get(),
                .index_buffer = frame_buffers.index_buffer.get(),
                .batches = stage_state.batches,
                .view_projection = stage_context.view_projection,
                .viewport = stage_context.viewport,
                .presentation_mask = presentation_mask
            };

            if (is_text)
                _rhi->record_text_quad_stage(record);
            else
                _rhi->record_textured_quad_stage(record);

            _stats.vertex_count += static_cast<uint32_t>(stage_state.vertices_cpu.size());
            _stats.index_count += static_cast<uint32_t>(stage_state.indices_cpu.size());
            _stats.textured_quad_batch_count += static_cast<uint32_t>(stage_state.batches.size());
            _stats.draw_calls += static_cast<uint32_t>(stage_state.batches.size());
        };

        record_stage(_stage_textured_quads[frame_stage_index(stage_plan.kind)], false);
        record_stage(_stage_text_quads[frame_stage_index(stage_plan.kind)], true);
    }

    void renderer_t::execute_frame_stages()
    {
        CE_ASSERT(_frame_stage_plan.size() == static_cast<size_t>(frame_stage_kind_t::count),
                  "Renderer frame stage plan size must match frame_stage_kind_t count");
        CE_ASSERT(_frame_stage_plan[0].kind == frame_stage_kind_t::world &&
                      _frame_stage_plan[0].space == frame_stage_space_t::world_camera,
                  "Renderer stage 0 must remain the world stage in world-camera space");
        CE_ASSERT(_frame_stage_plan[1].kind == frame_stage_kind_t::ui &&
                      _frame_stage_plan[1].space == frame_stage_space_t::render_target_pixels,
                  "Renderer stage 1 must remain the UI stage in render-target pixel space");
        CE_ASSERT(_frame_stage_plan[2].kind == frame_stage_kind_t::composite &&
                      _frame_stage_plan[2].space == frame_stage_space_t::render_target_pixels,
                  "Renderer stage 2 must remain the composite stage in render-target pixel space");
        CE_ASSERT(_frame_stage_plan[3].kind == frame_stage_kind_t::overlay_debug &&
                      _frame_stage_plan[3].space == frame_stage_space_t::viewport_pixels,
                  "Renderer stage 3 must remain the debug overlay stage in viewport pixel space");
        CE_ASSERT(_frame_stage_plan[4].kind == frame_stage_kind_t::log_console &&
                      _frame_stage_plan[4].space == frame_stage_space_t::render_target_pixels,
                  "Renderer stage 4 must remain the log console stage in render-target pixel space");

        _stats.vertex_count = 0;
        _stats.index_count = 0;
        _stats.textured_quad_batch_count = 0;
        _stats.draw_calls = 0;

        for (const frame_stage_plan_t& stage_plan : _frame_stage_plan)
            execute_frame_stage(stage_plan);
    }

    void renderer_t::release_frame_resources()
    {
        auto release_stage_buffers = [](textured_quad_state_t& stage_state)
        {
            stage_state.submissions.clear();
            stage_state.vertices_cpu.clear();
            stage_state.indices_cpu.clear();
            stage_state.batches.clear();
            for (auto& frame_buffers : stage_state.frame_buffers)
            {
                frame_buffers.vertex_buffer.reset();
                frame_buffers.index_buffer.reset();
                frame_buffers.vertex_capacity = 0;
                frame_buffers.index_capacity = 0;
            }
        };

        for (textured_quad_state_t& stage_state : _stage_textured_quads)
            release_stage_buffers(stage_state);

        for (textured_quad_state_t& stage_state : _stage_text_quads)
            release_stage_buffers(stage_state);

        _stats = { };
    }

    void renderer_t::ensure_textured_quad_frame_buffers(textured_quad_state_t& state)
    {
        auto& frame_buffers{ current_frame_buffers(state) };
        const size_t required_vertex_bytes{ state.vertices_cpu.size() * sizeof(quad_vertex_t) };
        const size_t required_index_bytes{ state.indices_cpu.size() * sizeof(uint32_t) };

        if (required_vertex_bytes == 0 || required_index_bytes == 0)
            return;

        const bool needs_vertex_realloc{
            frame_buffers.vertex_buffer != nullptr && frame_buffers.vertex_capacity < required_vertex_bytes
        };
        const bool needs_index_realloc{
            frame_buffers.index_buffer != nullptr && frame_buffers.index_capacity < required_index_bytes
        };

        if ((needs_vertex_realloc || needs_index_realloc) &&
            _rhi->get_graphics_api() == rhi::graphics_api::vulkan)
        {
            // The previous frame may still be using the current geometry buffers.
            // Wait before replacing them so Vulkan backends do not destroy in-flight buffers.
            _rhi->wait_idle();
        }

        if (frame_buffers.vertex_buffer == nullptr || frame_buffers.vertex_capacity < required_vertex_bytes)
        {
            rhi::buffer_create_info_t info{ };
            info.size_bytes = required_vertex_bytes;
            info.usage = rhi::buffer_usage_t::vertex;
            info.initial_data = nullptr;
            info.cpu_writable = true;

            frame_buffers.vertex_buffer = _rhi->create_buffer(info);
            if (!frame_buffers.vertex_buffer)
            {
                LOG_GRAPHICS_FATAL("Failed to create textured quad frame vertex buffer");
                frame_buffers.vertex_capacity = 0;
                return;
            }

            frame_buffers.vertex_capacity = required_vertex_bytes;
        }

        if (frame_buffers.index_buffer == nullptr || frame_buffers.index_capacity < required_index_bytes)
        {
            rhi::buffer_create_info_t info{ };
            info.size_bytes = required_index_bytes;
            info.usage = rhi::buffer_usage_t::index;
            info.initial_data = nullptr;
            info.cpu_writable = true;

            frame_buffers.index_buffer = _rhi->create_buffer(info);
            if (!frame_buffers.index_buffer)
            {
                LOG_GRAPHICS_FATAL("Failed to create textured quad frame index buffer");
                frame_buffers.index_capacity = 0;
                return;
            }

            frame_buffers.index_capacity = required_index_bytes;
        }
    }

    void renderer_t::upload_textured_quad_frame_data(const textured_quad_state_t& state) const
    {
        const auto& frame_buffers{ current_frame_buffers(state) };
        if (state.vertices_cpu.empty() || state.indices_cpu.empty())
            return;

        if (!frame_buffers.vertex_buffer || !frame_buffers.index_buffer)
        {
            LOG_GRAPHICS_FATAL("Textured quad frame buffers are not available for upload");
            return;
        }

        const size_t vertex_bytes{ state.vertices_cpu.size() * sizeof(quad_vertex_t) };
        const size_t index_bytes{ state.indices_cpu.size() * sizeof(uint32_t) };

        if (!frame_buffers.vertex_buffer->write(state.vertices_cpu.data(), vertex_bytes, 0))
        {
            LOG_GRAPHICS_FATAL("Failed to upload textured quad vertex data");
            return;
        }

        if (!frame_buffers.index_buffer->write(state.indices_cpu.data(), index_bytes, 0))
            LOG_GRAPHICS_FATAL("Failed to upload textured quad index data");
    }

    uint32_t renderer_t::current_textured_quad_frame_buffer_slot() const noexcept
    {
        return static_cast<uint32_t>(_frame_index % k_textured_quad_frame_buffer_count);
    }

    textured_quad_state_t::frame_buffers_t& renderer_t::current_frame_buffers(textured_quad_state_t& state) const noexcept
    {
        return state.frame_buffers[current_textured_quad_frame_buffer_slot()];
    }

    const textured_quad_state_t::frame_buffers_t& renderer_t::current_frame_buffers(const textured_quad_state_t& state) const noexcept
    {
        return state.frame_buffers[current_textured_quad_frame_buffer_slot()];
    }
} // namespace carrot::renderer
