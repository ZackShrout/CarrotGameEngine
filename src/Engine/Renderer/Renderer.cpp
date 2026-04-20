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
#include "RHI/SamplerPresets.h"
#include "RHI/Swapchain.h"
#include "Window/Window.h"
#include "World/World.h"
#include "World/WorldLayering.h"
#include "World/WorldUnits.h"

namespace carrot::renderer {
    struct renderer_t::world_stage_draw_context_t
    {
        const world::world_t* world{ nullptr };
        const world::world_presentation_t* presentation{ nullptr };
        std::vector<std::string_view> active_visibility_tags;
        world::layering_debug_snapshot_t layering_debug_snapshot{ };
    };

    namespace {
        [[nodiscard]] float luminance(const chlm::float3& color) noexcept
        {
            return color.x * 0.2126f + color.y * 0.7152f + color.z * 0.0722f;
        }

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

        [[nodiscard]] bool circle_overlaps_aabb(const chlm::float2 center,
                                                const float radius,
                                                const chlm::float2 aabb_min,
                                                const chlm::float2 aabb_max) noexcept
        {
            const chlm::float2 clamped{
                std::clamp(center.x, aabb_min.x, aabb_max.x),
                std::clamp(center.y, aabb_min.y, aabb_max.y)
            };
            const chlm::float2 delta{ center - clamped };
            return ((delta.x * delta.x) + (delta.y * delta.y)) <= (radius * radius);
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

        ensure_forward_plus_gpu_buffers();
        ensure_world_indirect_quad_buffers();

        const std::string_view forward_plus_shader_path{
            _rhi->get_graphics_api() == rhi::graphics_api::vulkan
                ? "engine://shaders/vulkan/forward_plus_classify.comp.spv"
                : (_rhi->get_graphics_api() == rhi::graphics_api::metal
                       ? "engine://shaders/metal/forward_plus_classify.comp.metallib"
                       : (_rhi->get_graphics_api() == rhi::graphics_api::direct_x12
                              ? "engine://shaders/dx12/forward_plus_classify.comp.dxil"
                              : (_rhi->get_graphics_api() == rhi::graphics_api::null_backend
                                     ? "null://forward_plus_classify.comp"
                                     : "")))
        };
        if (!forward_plus_shader_path.empty())
        {
            _forward_plus_classify_pipeline = _rhi->create_compute_pipeline({
                .shader_path = forward_plus_shader_path,
                .debug_name = "forward plus classify",
                .threadgroup_size_x = 64u,
                .max_constant_size_bytes = 0u
            });
        }

        const std::string_view world_item_cull_shader_path{
            _rhi->get_graphics_api() == rhi::graphics_api::vulkan
                ? "engine://shaders/vulkan/world_item_cull.comp.spv"
                : (_rhi->get_graphics_api() == rhi::graphics_api::metal
                       ? "engine://shaders/metal/world_item_cull.comp.metallib"
                       : (_rhi->get_graphics_api() == rhi::graphics_api::direct_x12
                              ? "engine://shaders/dx12/world_item_cull.comp.dxil"
                              : (_rhi->get_graphics_api() == rhi::graphics_api::null_backend
                                     ? "null://world_item_cull.comp"
                                     : "")))
        };
        if (!world_item_cull_shader_path.empty())
        {
            _world_item_cull_pipeline = _rhi->create_compute_pipeline({
                .shader_path = world_item_cull_shader_path,
                .debug_name = "world item cull",
                .threadgroup_size_x = 1u,
                .max_constant_size_bytes = 0u
            });
        }

        _is_initialized = true;
        validate_shared_renderer_limits();
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
        _world_indirect_quad_vertex_buffer.reset();
        _world_indirect_quad_index_buffer.reset();
        _forward_plus_classify_pipeline.reset();
        _world_item_cull_pipeline.reset();
        _shader_provider.reset();
        _rhi.reset();

        _is_initialized = false;
        LOG_GRAPHICS_INFO("Renderer shutdown complete");
    }

    void renderer_t::begin_frame()
    {
        validate_shared_renderer_limits();

        _frame_index++;
        _animated_tiles_elapsed_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - _animated_tiles_clock_origin).count());

        for (const frame_stage_plan_t& stage_plan : _frame_stage_plan)
            reset_stage_submission_group(stage_submission_group(stage_plan.kind));

        _stats = { };
        _transition_fade_enabled = false;
        _transition_fade_color = 0x00000000u;
        _transition_battle_swirl = { };
        _composite_overlay_enabled = false;
        _composite_overlay_color = 0x00000000u;
        _composite_fullscreen_passes.clear();
        _world_ambient_color = { 1.f, 1.f, 1.f, 1.f };
        _world_render_items.items.clear();
        _world_indirect_batches.clear();
        _world_forward_plus_light_input = { };
        _world_forward_plus_constants = { };
        _world_forward_plus_output = { };
        refresh_composite_targets();

        _rhi->begin_frame();
    }

    void renderer_t::end_frame()
    {
        queue_bloom_if_needed();
        queue_transition_fade_if_needed();
        queue_composite_overlay_if_needed();
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
        submit_world_textured_quad(quad);
    }

    void renderer_t::draw_overlay_textured_quad(const textured_quad_draw_info_t& quad)
    {
        submit_non_world_textured_quad(non_world_stage_target_t::overlay_debug, quad);
    }

    void renderer_t::draw_ui_textured_quad(const textured_quad_draw_info_t& quad)
    {
        submit_non_world_textured_quad(non_world_stage_target_t::ui, quad);
    }

    void renderer_t::draw_composite_textured_quad(const textured_quad_draw_info_t& quad)
    {
        submit_non_world_textured_quad(non_world_stage_target_t::composite, quad);
    }

    void renderer_t::draw_log_console_textured_quad(const textured_quad_draw_info_t& quad)
    {
        submit_non_world_textured_quad(non_world_stage_target_t::log_console, quad);
    }

    void renderer_t::draw_text_quad(const textured_quad_draw_info_t& quad)
    {
        submit_world_text_quad(quad);
    }

    void renderer_t::draw_overlay_text_quad(const textured_quad_draw_info_t& quad)
    {
        submit_non_world_text_quad(non_world_stage_target_t::overlay_debug, quad);
    }

    void renderer_t::draw_ui_text_quad(const textured_quad_draw_info_t& quad)
    {
        submit_non_world_text_quad(non_world_stage_target_t::ui, quad);
    }

    void renderer_t::draw_log_console_text_quad(const textured_quad_draw_info_t& quad)
    {
        submit_non_world_text_quad(non_world_stage_target_t::log_console, quad);
    }

    void renderer_t::draw_solid_quad(const solid_quad_draw_info_t& quad)
    {
        submit_solid_quad(frame_stage_kind_t::world, quad);
    }

    void renderer_t::draw_overlay_solid_quad(const solid_quad_draw_info_t& quad)
    {
        submit_non_world_solid_quad(non_world_stage_target_t::overlay_debug, quad);
    }

    void renderer_t::draw_ui_solid_quad(const solid_quad_draw_info_t& quad)
    {
        submit_non_world_solid_quad(non_world_stage_target_t::ui, quad);
    }

    void renderer_t::draw_composite_solid_quad(const solid_quad_draw_info_t& quad)
    {
        submit_non_world_solid_quad(non_world_stage_target_t::composite, quad);
    }

    void renderer_t::draw_log_console_solid_quad(const solid_quad_draw_info_t& quad)
    {
        submit_non_world_solid_quad(non_world_stage_target_t::log_console, quad);
    }

    void renderer_t::set_bloom_settings(const bloom_settings_t& settings) noexcept
    {
        _bloom_settings = settings;
    }

    void renderer_t::set_transition_fade_color(const uint32_t color_abgr) noexcept
    {
        _transition_fade_enabled = true;
        _transition_fade_color = color_abgr;
    }

    void renderer_t::clear_transition_fade() noexcept
    {
        _transition_fade_enabled = false;
        _transition_fade_color = 0x00000000u;
    }

    void renderer_t::set_transition_battle_swirl(const float progress, const bool incoming) noexcept
    {
        _transition_battle_swirl.enabled = true;
        _transition_battle_swirl.progress = std::clamp(progress, 0.f, 1.f);
        _transition_battle_swirl.incoming = incoming;
    }

    void renderer_t::clear_transition_battle_swirl() noexcept
    {
        _transition_battle_swirl = { };
    }

    void renderer_t::set_composite_overlay_color(const uint32_t color_abgr) noexcept
    {
        _composite_overlay_enabled = true;
        _composite_overlay_color = color_abgr;
    }

    void renderer_t::clear_composite_overlay() noexcept
    {
        _composite_overlay_enabled = false;
        _composite_overlay_color = 0x00000000u;
    }

    void renderer_t::submit_world_textured_quad(const textured_quad_draw_info_t& quad)
    {
        if (quad.texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_textured_quad called with null texture");
            return;
        }

        extract_world_render_item(quad, {
            .domain = world_material_domain_t::lit,
            .feature_flags = 0u
        });

        _stats.textured_quad_count++;
    }

    void renderer_t::extract_world_render_item(const textured_quad_draw_info_t& quad,
                                               const world_material_key_t world_material)
    {
        _world_render_items.items.push_back({
            .texture = quad.texture,
            .position_px = { quad.x, quad.y },
            .size_px = { quad.width, quad.height },
            .uv_rect = {
                .u_min = quad.u0,
                .v_min = quad.v0,
                .u_max = quad.u1,
                .v_max = quad.v1
            },
            .layer = quad.layer,
            .order_mode = quad.order_mode,
            .order_in_layer = quad.order_in_layer,
            .sort_reference_y = quad.sort_reference_y,
            .color = quad.color,
            .effect_mode = quad.effect_mode,
            .effect_param0 = quad.effect_param0,
            .sampler_preset = quad.sampler_preset,
            .world_material = world_material,
            .bounds_min_px = { quad.x, quad.y },
            .bounds_max_px = { quad.x + quad.width, quad.y + quad.height },
            .submission_index = static_cast<std::uint64_t>(_world_render_items.items.size())
        });
    }

    void renderer_t::submit_stage_textured_quad(const frame_stage_plan_t& stage_plan, const textured_quad_draw_info_t& quad)
    {
        if (quad.texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_textured_quad called with null texture");
            return;
        }

        CE_ASSERT(stage_plan.kind != frame_stage_kind_t::world,
                  "Renderer non-world textured quad submission must not target the world stage");

        textured_quad_state_t& stage_state{ _stage_textured_quads[frame_stage_index(stage_plan.kind)] };
        stage_state.submissions.push_back({
            .quad = quad,
            .world_material = {
                .domain = world_material_domain_t::unlit,
                .feature_flags = 0u
            },
            .submission_index = static_cast<uint64_t>(stage_state.submissions.size())
        });

        _stats.textured_quad_count++;
    }

    void renderer_t::submit_non_world_textured_quad(const non_world_stage_target_t target,
                                                    const textured_quad_draw_info_t& quad)
    {
        submit_stage_textured_quad(non_world_stage_plan(target), quad);
    }

    void renderer_t::submit_solid_quad(const frame_stage_kind_t stage, const solid_quad_draw_info_t& quad)
    {
        if (_solid_white_texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_solid_quad called before renderer solid white texture was ready");
            return;
        }

        const textured_quad_draw_info_t textured_quad{
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
        };

        if (stage == frame_stage_kind_t::world)
        {
            submit_world_textured_quad(textured_quad);
            return;
        }

        submit_stage_textured_quad(stage_plan(stage), textured_quad);
    }

    void renderer_t::submit_world_text_quad(const textured_quad_draw_info_t& quad)
    {
        if (quad.texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_text_quad called with null texture");
            return;
        }

        _world_text_quads.submissions.push_back({
            .quad = quad,
            .world_material = {
                .domain = world_material_domain_t::unlit,
                .feature_flags = 0u
            },
            .submission_index = static_cast<uint64_t>(_world_text_quads.submissions.size())
        });

        _stats.textured_quad_count++;
    }

    void renderer_t::submit_stage_text_quad(const frame_stage_plan_t& stage_plan, const textured_quad_draw_info_t& quad)
    {
        if (quad.texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_text_quad called with null texture");
            return;
        }

        CE_ASSERT(stage_plan.kind != frame_stage_kind_t::world,
                  "Renderer non-world text quad submission must not target the world stage");

        textured_quad_state_t& stage_state{ _stage_text_quads[frame_stage_index(stage_plan.kind)] };
        stage_state.submissions.push_back({
            .quad = quad,
            .world_material = {
                .domain = world_material_domain_t::unlit,
                .feature_flags = 0u
            },
            .submission_index = static_cast<uint64_t>(stage_state.submissions.size())
        });

        _stats.textured_quad_count++;
    }

    void renderer_t::submit_non_world_text_quad(const non_world_stage_target_t target,
                                                const textured_quad_draw_info_t& quad)
    {
        submit_stage_text_quad(non_world_stage_plan(target), quad);
    }

    void renderer_t::submit_non_world_solid_quad(const non_world_stage_target_t target,
                                                 const solid_quad_draw_info_t& quad)
    {
        submit_solid_quad(non_world_stage_plan(target).kind, quad);
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
        const resolved_camera_2d_t resolved_world_camera{ _active_camera.resolve(current_render_target_size()) };
        const chlm::float2 visible_min_px{ _active_camera.position.x, _active_camera.position.y };
        const chlm::float2 visible_max_px{
            visible_min_px.x + resolved_world_camera.visible_world_size.x,
            visible_min_px.y + resolved_world_camera.visible_world_size.y
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

                const auto layer_chunks{ info.tilemap->tile_render_chunks_for_layer(layer_index) };
                for (const assets::tilemap_render_chunk_t& chunk : layer_chunks)
                {
                    const chlm::float2 chunk_min_px{
                        info.origin.x + (static_cast<float>(chunk.tile_min_x) * render_tile_size.x * info.scale.x),
                        info.origin.y + (static_cast<float>(chunk.tile_min_y) * render_tile_size.y * info.scale.y)
                    };
                    const chlm::float2 chunk_max_px{
                        info.origin.x + (static_cast<float>(chunk.tile_max_x) * render_tile_size.x * info.scale.x),
                        info.origin.y + (static_cast<float>(chunk.tile_max_y) * render_tile_size.y * info.scale.y)
                    };

                    const bool chunk_visible{
                        chunk_max_px.x > visible_min_px.x &&
                        chunk_max_px.y > visible_min_px.y &&
                        chunk_min_px.x < visible_max_px.x &&
                        chunk_min_px.y < visible_max_px.y
                    };
                    if (!chunk_visible)
                        continue;

                    for (const std::uint32_t cell_index : chunk.occupied_cell_indices)
                    {
                        if (cell_index >= layer.gids.size())
                            continue;

                        const std::uint32_t row{ cell_index / layer.width };
                        const std::uint32_t col{ cell_index % layer.width };
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
        world_stage_draw_context_t context;
        prepare_world_stage_context(world, context);

        for (const world::world_object_t& object : world.objects())
            submit_world_object(object, context);

        _stats.active_visibility_tag_count = static_cast<std::uint32_t>(context.layering_debug_snapshot.active_visibility_tags.size());
        _stats.visible_layer_count = context.layering_debug_snapshot.visible_layer_count;
        _stats.hidden_layer_count = context.layering_debug_snapshot.hidden_layer_count;

        finalize_world_stage_context(context);
    }

    void renderer_t::prepare_world_stage_context(const world::world_t& world, world_stage_draw_context_t& context)
    {
        context.world = &world;
        context.presentation = &world.presentation();
        context.active_visibility_tags.clear();
        context.layering_debug_snapshot = world::layering_debug_snapshot_t{ };
        context.layering_debug_snapshot.frame_index = _frame_index;

        const world::world_presentation_t& presentation{ *context.presentation };
        _world_ambient_color = world.lighting().ambient_color;
        _world_forward_plus_light_input = { };
        _stats.dropped_world_point_light_count = 0u;
        for (const world::world_lighting_state_t::point_light_t& light : world.lighting().point_lights)
        {
            if (_world_forward_plus_constants.point_light_counts[0] >= k_max_world_point_lights)
            {
                _stats.dropped_world_point_light_count++;
                continue;
            }

            const chlm::float2 light_position_px{ presentation.world_position_to_pixels(light.position_world) };
            const chlm::float2 light_radius_px{ presentation.world_size_to_pixels({ light.radius_world, light.radius_world }) };

            _world_forward_plus_light_input.point_lights[_world_forward_plus_constants.point_light_counts[0]++] =
                world_point_light_uniform_t{
                .position_radius_px = { light_position_px.x, light_position_px.y, light_radius_px.x, 0.f },
                .color_intensity = { light.color.x, light.color.y, light.color.z, light.intensity }
                };
        }

        if (const world::world_object_t* visibility_anchor{ resolve_primary_visibility_anchor(world) };
            visibility_anchor && visibility_anchor->transform)
        {
            context.layering_debug_snapshot.has_visibility_anchor = true;
            context.layering_debug_snapshot.visibility_anchor_world = visibility_anchor->transform->position;
            context.active_visibility_tags = world.collect_active_visibility_tags(visibility_anchor->transform->position);
        }
        context.layering_debug_snapshot.active_visibility_tags.reserve(context.active_visibility_tags.size());
        for (const std::string_view tag : context.active_visibility_tags)
            context.layering_debug_snapshot.active_visibility_tags.emplace_back(tag);

        for (const world::world_object_t& object : world.objects())
        {
            if (object.visibility_region)
                context.layering_debug_snapshot.visibility_region_count++;
        }
    }

    void renderer_t::submit_world_object(const world::world_object_t& object, world_stage_draw_context_t& context)
    {
        CE_ASSERT(context.world != nullptr && context.presentation != nullptr,
                  "Renderer world stage context must be prepared before submitting world objects");

        if (!object.transform || (!object.sprite && !object.tilemap && !object.tile_object))
            return;

        const world::transform_component_t& transform{ *object.transform };
        const chlm::float2 render_position_px{
            context.presentation->world_position_to_pixels(transform.position)
        };

        if (object.tilemap)
        {
            const world::tilemap_component_t& tilemap{ *object.tilemap };
            submit_tilemap({
                .tilemap = tilemap.tilemap,
                .origin = render_position_px,
                .scale = transform.scale,
                .source_pixels_per_unit = world::world_units_t::default_pixels_per_unit,
                .render_pixels_per_unit = context.presentation->pixels_per_unit,
                .include_object_layers = tilemap.include_object_layers,
                .active_visibility_tags = context.active_visibility_tags,
                .layer = tilemap.layer,
                .order_mode = tilemap.order_mode,
                .order_in_layer = tilemap.order_in_layer,
                .sort_reference_y = tilemap.sort_reference_y,
                .sampler_preset = tilemap.sampler_preset,
                .color = tilemap.color
            }, &context.layering_debug_snapshot);
        }

        if (object.tile_object)
        {
            const world::tile_object_component_t& tile_object{ *object.tile_object };
            if (tile_object.tilemap && tile_object.gid != 0)
            {
                const chlm::float2 world_size{
                    world::world_units_t::pixel_size_to_world(tile_object.size_source_px,
                                                              world::world_units_t::default_pixels_per_unit)
                };
                const chlm::float2 render_size_px{ context.presentation->world_size_to_pixels(world_size) };

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
        }

        if (object.sprite)
        {
            const world::sprite_component_t& sprite{ *object.sprite };
            const assets::sprite_frame_t* frame{
                object.sprite_animator ? object.sprite_animator->animator.current_frame() : sprite.frame
            };

            if (sprite.sprite && frame)
            {
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
                const chlm::float2 render_size_px{ context.presentation->world_size_to_pixels(final_world_size) };

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
        }

        if (object.visibility_region && object.transform && context.world->layering_debug_view().show_visibility_regions)
        {
            const chlm::float2 render_size_px{
                context.presentation->world_size_to_pixels(object.visibility_region->size_world)
            };
            draw_overlay_solid_quad({
                .x = render_position_px.x,
                .y = render_position_px.y,
                .width = render_size_px.x,
                .height = render_size_px.y,
                .layer = render_layer_t::debug,
                .order_mode = render_order_mode_t::explicit_order,
                .order_in_layer = 0,
                .color = context.world->layering_debug_view().visibility_region_color,
                .sampler_preset = quad_sampler_preset_t::pixel_clamp
            });
        }
    }

    void renderer_t::finalize_world_stage_context(world_stage_draw_context_t& context) const
    {
        CE_ASSERT(context.world != nullptr,
                  "Renderer world stage context must reference a world before finalization");
        context.world->set_layering_debug_snapshot(std::move(context.layering_debug_snapshot));
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

    const renderer_t::frame_stage_plan_t& renderer_t::stage_plan(const frame_stage_kind_t stage) const noexcept
    {
        return _frame_stage_plan[frame_stage_index(stage)];
    }

    composite_target_t renderer_t::composite_target(const composite_target_kind_t kind) const noexcept
    {
        switch (kind)
        {
            case composite_target_kind_t::gameplay_present:
            default:
                return _composite_targets[0u];
        }
    }

    const renderer_t::frame_stage_plan_t& renderer_t::non_world_stage_plan(
        const non_world_stage_target_t target) const noexcept
    {
        switch (target)
        {
            case non_world_stage_target_t::ui:
                return stage_plan(frame_stage_kind_t::ui);
            case non_world_stage_target_t::composite:
                return stage_plan(frame_stage_kind_t::composite);
            case non_world_stage_target_t::overlay_debug:
                return stage_plan(frame_stage_kind_t::overlay_debug);
            case non_world_stage_target_t::log_console:
                return stage_plan(frame_stage_kind_t::log_console);
            default:
                CE_ASSERT(false, "Renderer non-world stage target must resolve to a known frame stage");
                return stage_plan(frame_stage_kind_t::ui);
        }
    }

    renderer_t::stage_submission_group_t renderer_t::stage_submission_group(const frame_stage_kind_t stage) noexcept
    {
        if (stage == frame_stage_kind_t::world)
        {
            return {
                .textured = nullptr,
                .text = &_world_text_quads
            };
        }

        return {
            .textured = &_stage_textured_quads[frame_stage_index(stage)],
            .text = &_stage_text_quads[frame_stage_index(stage)]
        };
    }

    void renderer_t::validate_frame_stage_plan() const noexcept
    {
        validate_shared_renderer_limits();

        CE_ASSERT(_frame_stage_plan.size() == static_cast<size_t>(frame_stage_kind_t::count),
                  "Renderer frame stage plan size must match frame_stage_kind_t count");

        const frame_stage_plan_t& world_stage{ stage_plan(frame_stage_kind_t::world) };
        CE_ASSERT(world_stage.kind == frame_stage_kind_t::world &&
                      world_stage.space == frame_stage_space_t::world_camera &&
                      world_stage.presentation_mask == rhi::presentation_channel_gameplay &&
                      rhi::presentation_mask_uses_known_channels(world_stage.presentation_mask) &&
                      world_stage.lighting_aware,
                  "Renderer world stage must remain gameplay-presented, world-camera, and lighting-aware");

        const frame_stage_plan_t& ui_stage{ stage_plan(frame_stage_kind_t::ui) };
        CE_ASSERT(ui_stage.kind == frame_stage_kind_t::ui &&
                      ui_stage.space == frame_stage_space_t::render_target_pixels &&
                      ui_stage.presentation_mask == rhi::presentation_channel_gameplay &&
                      rhi::presentation_mask_uses_known_channels(ui_stage.presentation_mask) &&
                      !ui_stage.lighting_aware,
                  "Renderer UI stage must remain gameplay-presented, render-target-pixel, and unlit");

        const frame_stage_plan_t& composite_stage{ stage_plan(frame_stage_kind_t::composite) };
        CE_ASSERT(composite_stage.kind == frame_stage_kind_t::composite &&
                      composite_stage.space == frame_stage_space_t::render_target_pixels &&
                      composite_stage.presentation_mask == rhi::presentation_channel_gameplay &&
                      rhi::presentation_mask_uses_known_channels(composite_stage.presentation_mask) &&
                      !composite_stage.lighting_aware,
                  "Renderer composite stage must remain gameplay-presented, render-target-pixel, and unlit");

        const frame_stage_plan_t& overlay_stage{ stage_plan(frame_stage_kind_t::overlay_debug) };
        CE_ASSERT(overlay_stage.kind == frame_stage_kind_t::overlay_debug &&
                      overlay_stage.space == frame_stage_space_t::viewport_pixels &&
                      overlay_stage.presentation_mask == rhi::presentation_channel_gameplay &&
                      rhi::presentation_mask_uses_known_channels(overlay_stage.presentation_mask) &&
                      !overlay_stage.lighting_aware,
                  "Renderer overlay debug stage must remain gameplay-presented, viewport-pixel, and unlit");

        const frame_stage_plan_t& log_console_stage{ stage_plan(frame_stage_kind_t::log_console) };
        CE_ASSERT(log_console_stage.kind == frame_stage_kind_t::log_console &&
                      log_console_stage.space == frame_stage_space_t::render_target_pixels &&
                      log_console_stage.presentation_mask == rhi::presentation_channel_log_console &&
                      rhi::presentation_mask_uses_known_channels(log_console_stage.presentation_mask) &&
                      !log_console_stage.lighting_aware,
                  "Renderer log console stage must remain log-console-presented, render-target-pixel, and unlit");
    }

    void renderer_t::validate_shared_renderer_limits() const noexcept
    {
        CE_ASSERT(rhi::k_max_textured_quad_stage_slots_per_frame >= static_cast<uint32_t>(frame_stage_kind_t::count),
                  "Renderer stage-slot budget must cover every declared frame stage");
        CE_ASSERT(k_max_world_point_lights > 0u,
                  "Renderer forward+ lighting contract requires at least one world point-light slot");
        CE_ASSERT(k_max_forward_plus_tiles == (k_max_forward_plus_tiles_x * k_max_forward_plus_tiles_y),
                  "Renderer forward+ tile budget must remain derived from x/y tile caps");
        CE_ASSERT(rhi::presentation_channel_gameplay != 0u &&
                      rhi::presentation_channel_log_console != 0u &&
                      rhi::presentation_channel_gameplay != rhi::presentation_channel_log_console &&
                      rhi::presentation_mask_uses_known_channels(rhi::k_known_presentation_channel_mask),
                  "Renderer presentation-channel routing must use distinct known shared channels");
    }

    void renderer_t::refresh_composite_targets() noexcept
    {
        const chlm::uint2 render_target_size{ current_render_target_size() };
        _composite_targets[0u] = composite_target_t{
            .kind = composite_target_kind_t::gameplay_present,
            .pixel_size = {
                std::max(1u, render_target_size.x),
                std::max(1u, render_target_size.y)
            },
            .rect_px = {
                .position = { 0u, 0u },
                .size = {
                    std::max(1u, render_target_size.x),
                    std::max(1u, render_target_size.y)
                }
            },
            .debug_name = "gameplay_present"
        };
    }

    float renderer_t::resolve_bloom_strength() const noexcept
    {
        if (!_bloom_settings.enabled)
            return 0.0f;

        float peak_light_luma{ 0.0f };
        float accumulated_light_luma{ 0.0f };
        const std::uint32_t point_light_count{ _world_forward_plus_constants.point_light_counts[0] };
        for (std::uint32_t i = 0u; i < point_light_count; ++i)
        {
            const world_point_light_uniform_t& light{ _world_forward_plus_light_input.point_lights[i] };
            const float light_luma{
                luminance(chlm::float3{
                    light.color_intensity.x,
                    light.color_intensity.y,
                    light.color_intensity.z
                }) * std::max(0.0f, light.color_intensity.w)
            };
            peak_light_luma = std::max(peak_light_luma, light_luma);
            accumulated_light_luma += light_luma;
        }

        const float ambient_excess{
            std::max(0.0f,
                     luminance(chlm::float3{
                         _world_ambient_color.x,
                         _world_ambient_color.y,
                         _world_ambient_color.z
                     }) - 1.0f)
        };

        const float strength{
            _bloom_settings.baseline_strength +
            _bloom_settings.peak_light_response * std::clamp(peak_light_luma, 0.0f, 1.0f) +
            _bloom_settings.accumulated_light_response * std::clamp(accumulated_light_luma / 4.0f, 0.0f, 1.0f) +
            _bloom_settings.ambient_response * ambient_excess
        };
        return std::clamp(strength, 0.0f, std::max(0.0f, _bloom_settings.max_strength));
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

    void renderer_t::queue_composite_overlay_if_needed()
    {
        if (!_composite_overlay_enabled || _solid_white_texture == nullptr)
            return;

        queue_composite_fullscreen_textured_pass(composite_target_kind_t::gameplay_present,
                                                 textured_quad_draw_info_t{
            .texture = _solid_white_texture.get(),
            .x = 0.f,
            .y = 0.f,
            .width = 1.f,
            .height = 1.f,
            .u0 = 0.f,
            .v0 = 0.f,
            .u1 = 1.f,
            .v1 = 1.f,
            .layer = render_layer_t::ui,
            .order_in_layer = 0,
            .color = _composite_overlay_color,
            .sampler_preset = quad_sampler_preset_t::pixel_clamp
        },
                                                 "composite_overlay");
    }

    void renderer_t::queue_transition_fade_if_needed()
    {
        if (!_transition_fade_enabled || _solid_white_texture == nullptr)
            return;

        queue_composite_fullscreen_textured_pass(composite_target_kind_t::gameplay_present,
                                                 textured_quad_draw_info_t{
            .texture = _solid_white_texture.get(),
            .x = 0.f,
            .y = 0.f,
            .width = 1.f,
            .height = 1.f,
            .u0 = 0.f,
            .v0 = 0.f,
            .u1 = 1.f,
            .v1 = 1.f,
            .layer = render_layer_t::ui,
            .order_in_layer = 0,
            .color = _transition_fade_color,
            .sampler_preset = quad_sampler_preset_t::pixel_clamp
        },
                                                 "transition_fade");
    }

    void renderer_t::record_transition_battle_swirl_if_needed(const stage_execution_context_t& stage_context,
                                                              const uint32_t presentation_mask)
    {
        if (!_transition_battle_swirl.enabled)
            return;

        ensure_transition_battle_swirl_quad_buffers();
        ensure_transition_battle_swirl_capture_texture();

        if (!_rhi ||
            !_transition_battle_swirl_vertex_buffer ||
            !_transition_battle_swirl_index_buffer ||
            !_transition_battle_swirl_capture_texture)
        {
            return;
        }

        if (!_rhi->get_or_create_sampler(rhi::sampler_desc_from_preset(quad_sampler_preset_t::smooth_clamp)))
        {
            LOG_GRAPHICS_FATAL("Failed to resolve transition battle swirl sampler");
            return;
        }

        constexpr std::array<std::uint32_t, 6u> quad_indices{ 0u, 1u, 2u, 0u, 2u, 3u };
        const float effect_mode{
            _transition_battle_swirl.incoming
                ? renderer::k_effect_mode_battle_swirl_in
                : renderer::k_effect_mode_battle_swirl_out
        };
        const float x0{ static_cast<float>(stage_context.viewport.rect_px.position.x) };
        const float y0{ static_cast<float>(stage_context.viewport.rect_px.position.y) };
        const float x1{ x0 + static_cast<float>(stage_context.viewport.rect_px.size.x) };
        const float y1{ y0 + static_cast<float>(stage_context.viewport.rect_px.size.y) };
        const std::array<quad_vertex_t, 4u> quad_vertices{
            quad_vertex_t{ .x = x0, .y = y0, .u = 0.f, .v = 0.f, .color = 0xFFFFFFFFu, .effect_mode = effect_mode, .effect_param0 = _transition_battle_swirl.progress },
            quad_vertex_t{ .x = x1, .y = y0, .u = 1.f, .v = 0.f, .color = 0xFFFFFFFFu, .effect_mode = effect_mode, .effect_param0 = _transition_battle_swirl.progress },
            quad_vertex_t{ .x = x1, .y = y1, .u = 1.f, .v = 1.f, .color = 0xFFFFFFFFu, .effect_mode = effect_mode, .effect_param0 = _transition_battle_swirl.progress },
            quad_vertex_t{ .x = x0, .y = y1, .u = 0.f, .v = 1.f, .color = 0xFFFFFFFFu, .effect_mode = effect_mode, .effect_param0 = _transition_battle_swirl.progress }
        };

        if (!_transition_battle_swirl_vertex_buffer->write(quad_vertices.data(), sizeof(quad_vertices), 0u) ||
            !_transition_battle_swirl_index_buffer->write(quad_indices.data(), sizeof(quad_indices), 0u))
        {
            LOG_GRAPHICS_FATAL("Failed to upload transition battle swirl fullscreen quad");
            return;
        }

        _transition_battle_swirl_batches[0] = textured_quad_batch_t{
            .texture = _transition_battle_swirl_capture_texture.get(),
            .first_index = 0u,
            .index_count = 6u,
            .sampler_preset = quad_sampler_preset_t::smooth_clamp,
            .world_material = { .domain = world_material_domain_t::unlit, .feature_flags = 0u }
        };

        _rhi->record_textured_quad_stage({
            .vertex_buffer = _transition_battle_swirl_vertex_buffer.get(),
            .index_buffer = _transition_battle_swirl_index_buffer.get(),
            .batches = _transition_battle_swirl_batches,
            .view_projection = stage_context.view_projection,
            .ambient_color = { 1.f, 1.f, 1.f, 1.f },
            .forward_plus_constants = { },
            .forward_plus_light_input = { },
            .forward_plus_output = { },
            .viewport = stage_context.viewport,
            .presentation_mask = presentation_mask,
            .capture_presentation_before_draw = true
        });

        _stats.draw_calls++;
        _stats.textured_quad_batch_count++;
        _stats.vertex_count += 4u;
        _stats.index_count += 6u;
    }

    void renderer_t::queue_bloom_if_needed()
    {
        const float strength{ resolve_bloom_strength() };
        if (strength <= 0.001f)
            return;

        const std::uint32_t alpha{
            static_cast<std::uint32_t>(std::round(std::clamp(strength, 0.0f, 1.0f) * 255.0f)) & 0xFFu
        };
        const std::uint32_t bloom_color{
            (_bloom_settings.tint_abgr & 0x00FFFFFFu) | (alpha << 24u)
        };

        queue_composite_fullscreen_solid_pass(composite_target_kind_t::gameplay_present,
                                              solid_quad_draw_info_t{
            .x = 0.0f,
            .y = 0.0f,
            .width = 1.0f,
            .height = 1.0f,
            .layer = render_layer_t::ui,
            .order_in_layer = 0,
            .color = bloom_color
        },
                                              "bloom_overlay");
        _stats.bloom_pass_count++;
    }

    void renderer_t::queue_composite_fullscreen_textured_pass(const composite_target_kind_t target_kind,
                                                              textured_quad_draw_info_t quad,
                                                              const char* const debug_name)
    {
        const composite_target_t target{ composite_target(target_kind) };
        quad.x = static_cast<float>(target.rect_px.position.x);
        quad.y = static_cast<float>(target.rect_px.position.y);
        quad.width = static_cast<float>(target.rect_px.size.x);
        quad.height = static_cast<float>(target.rect_px.size.y);
        quad.layer = render_layer_t::ui;
        quad.order_mode = render_order_mode_t::explicit_order;
        quad.order_in_layer = 0;
        quad.sort_reference_y = 0.f;

        _composite_fullscreen_passes.push_back(composite_fullscreen_pass_t{
            .kind = composite_fullscreen_pass_kind_t::textured,
            .target = target_kind,
            .textured_quad = quad,
            .solid_quad = { },
            .debug_name = debug_name
        });
    }

    void renderer_t::queue_composite_fullscreen_solid_pass(const composite_target_kind_t target_kind,
                                                           solid_quad_draw_info_t quad,
                                                           const char* const debug_name)
    {
        const composite_target_t target{ composite_target(target_kind) };
        quad.x = static_cast<float>(target.rect_px.position.x);
        quad.y = static_cast<float>(target.rect_px.position.y);
        quad.width = static_cast<float>(target.rect_px.size.x);
        quad.height = static_cast<float>(target.rect_px.size.y);
        quad.layer = render_layer_t::ui;
        quad.order_mode = render_order_mode_t::explicit_order;
        quad.order_in_layer = 0;
        quad.sort_reference_y = 0.f;

        _composite_fullscreen_passes.push_back(composite_fullscreen_pass_t{
            .kind = composite_fullscreen_pass_kind_t::solid,
            .target = target_kind,
            .textured_quad = { },
            .solid_quad = quad,
            .debug_name = debug_name
        });
    }

    void renderer_t::materialize_composite_fullscreen_passes()
    {
        for (const composite_fullscreen_pass_t& pass : _composite_fullscreen_passes)
        {
            switch (pass.kind)
            {
                case composite_fullscreen_pass_kind_t::textured:
                    submit_non_world_textured_quad(non_world_stage_target_t::composite, pass.textured_quad);
                    break;
                case composite_fullscreen_pass_kind_t::solid:
                    submit_non_world_solid_quad(non_world_stage_target_t::composite, pass.solid_quad);
                    break;
                default:
                    CE_ASSERT(false, "Composite fullscreen pass kind must be known");
                    break;
            }
        }

        _stats.composite_fullscreen_pass_count += static_cast<uint32_t>(_composite_fullscreen_passes.size());
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
                state.batches.back().sampler_preset != quad.sampler_preset ||
                !(state.batches.back().world_material == submission.world_material))
            {
                state.batches.push_back(textured_quad_batch_t{
                    .texture = quad.texture,
                    .first_index = static_cast<uint32_t>(state.indices_cpu.size()),
                    .index_count = 0,
                    .sampler_preset = quad.sampler_preset,
                    .world_material = submission.world_material
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

    void renderer_t::reset_stage_submission_group(const stage_submission_group_t& group) noexcept
    {
        const auto reset_state = [](textured_quad_state_t* state) noexcept
        {
            if (!state)
                return;

            state->submissions.clear();
            state->vertices_cpu.clear();
            state->indices_cpu.clear();
            state->batches.clear();
        };

        reset_state(group.textured);
        reset_state(group.text);
    }

    void renderer_t::record_stage_state(textured_quad_state_t& stage_state,
                                        const rhi::textured_quad_stage_record_t& record,
                                        const bool is_text)
    {
        build_textured_quad_batches(stage_state);

        if (stage_state.batches.empty())
            return;

        ensure_textured_quad_frame_buffers(stage_state);
        upload_textured_quad_frame_data(stage_state);

        const auto& frame_buffers{ current_frame_buffers(stage_state) };
        if (!frame_buffers.vertex_buffer || !frame_buffers.index_buffer)
            return;

        rhi::textured_quad_stage_record_t final_record{ record };
        final_record.vertex_buffer = frame_buffers.vertex_buffer.get();
        final_record.index_buffer = frame_buffers.index_buffer.get();
        final_record.batches = stage_state.batches;

        if (is_text)
            _rhi->record_text_quad_stage(final_record);
        else
            _rhi->record_textured_quad_stage(final_record);

        _stats.vertex_count += static_cast<uint32_t>(stage_state.vertices_cpu.size());
        _stats.index_count += static_cast<uint32_t>(stage_state.indices_cpu.size());
        _stats.textured_quad_batch_count += static_cast<uint32_t>(stage_state.batches.size());
        _stats.draw_calls += static_cast<uint32_t>(stage_state.batches.size());
    }

    void renderer_t::execute_world_frame_stage()
    {
        const frame_stage_plan_t& world_stage_plan{ stage_plan(frame_stage_kind_t::world) };
        const stage_execution_context_t stage_context{ resolve_stage_execution_context(world_stage_plan) };
        const resolved_camera_2d_t resolved_world_camera{ _active_camera.resolve(current_render_target_size()) };
        const std::uint32_t point_light_count{ _world_forward_plus_constants.point_light_counts[0] };

        _world_forward_plus_constants.grid_params = {
            _active_camera.position.x,
            _active_camera.position.y,
            static_cast<float>(k_forward_plus_tile_size_px),
            0.f
        };
        _world_forward_plus_constants.tile_counts = { 0u, 0u, 0u, 0u };
        _world_forward_plus_constants.point_light_counts = { point_light_count, 0u, 0u, 0u };
        _world_forward_plus_output = { };

        const std::uint32_t tile_count_x{
            std::min<std::uint32_t>(
                static_cast<std::uint32_t>(k_max_forward_plus_tiles_x),
                std::max(1u, static_cast<std::uint32_t>(
                    std::ceil(resolved_world_camera.visible_world_size.x / static_cast<float>(k_forward_plus_tile_size_px))))
            )
        };
        const std::uint32_t tile_count_y{
            std::min<std::uint32_t>(
                static_cast<std::uint32_t>(k_max_forward_plus_tiles_y),
                std::max(1u, static_cast<std::uint32_t>(
                    std::ceil(resolved_world_camera.visible_world_size.y / static_cast<float>(k_forward_plus_tile_size_px))))
            )
        };
        _world_forward_plus_constants.tile_counts = {
            tile_count_x,
            tile_count_y,
            static_cast<std::uint32_t>(k_forward_plus_tile_size_px),
            0u
        };
        _stats.world_point_light_count = _world_forward_plus_constants.point_light_counts[0];
        _stats.forward_plus_tile_count = tile_count_x * tile_count_y;
        _stats.forward_plus_light_index_count = 0u;
        _stats.forward_plus_dropped_light_references = 0u;
        update_forward_plus_diagnostics();

        upload_forward_plus_gpu_data();
        if (_forward_plus_classify_pipeline)
        {
            const forward_plus_gpu_buffers_t& gpu_buffers{ current_forward_plus_gpu_buffers() };
            const std::uint32_t tile_count{ tile_count_x * tile_count_y };
            const std::uint32_t group_count_x{ std::max(1u, (tile_count + 63u) / 64u) };
            const std::array<rhi::compute_buffer_binding_t, 2> read_only_bindings{
                rhi::compute_buffer_binding_t{ .slot = 0u, .buffer = gpu_buffers.constants_buffer.get() },
                rhi::compute_buffer_binding_t{ .slot = 1u, .buffer = gpu_buffers.light_input_buffer.get() }
            };
            const std::array<rhi::compute_buffer_binding_t, 1> storage_bindings{
                rhi::compute_buffer_binding_t{ .slot = 2u, .buffer = gpu_buffers.classification_output_buffer.get() }
            };

            _rhi->dispatch_compute({
                .pipeline = _forward_plus_classify_pipeline.get(),
                .read_only_buffers = read_only_bindings,
                .storage_buffers = storage_bindings,
                .graphics_handoff = rhi::compute_graphics_handoff_t::storage_write_to_graphics_read,
                .group_count_x = group_count_x,
                .group_count_y = 1u,
                .group_count_z = 1u
            });
        }

        const stage_submission_group_t group{ stage_submission_group(frame_stage_kind_t::world) };
        CE_ASSERT(group.text != nullptr, "Renderer world stage must have text submission state");
        const forward_plus_gpu_buffers_t& gpu_buffers{ current_forward_plus_gpu_buffers() };
        std::stable_sort(_world_render_items.items.begin(), _world_render_items.items.end(),
                         [](const world_render_item_t& lhs, const world_render_item_t& rhs) noexcept
                         {
                             textured_quad_draw_info_t lhs_quad{ };
                             lhs_quad.layer = lhs.layer;
                             lhs_quad.order_mode = lhs.order_mode;
                             lhs_quad.order_in_layer = lhs.order_in_layer;
                             lhs_quad.sort_reference_y = lhs.sort_reference_y;

                             textured_quad_draw_info_t rhs_quad{ };
                             rhs_quad.layer = rhs.layer;
                             rhs_quad.order_mode = rhs.order_mode;
                             rhs_quad.order_in_layer = rhs.order_in_layer;
                             rhs_quad.sort_reference_y = rhs.sort_reference_y;

                             if (quad_sorts_before(lhs_quad, rhs_quad))
                                 return true;

                             if (quad_sorts_before(rhs_quad, lhs_quad))
                                 return false;

                             return lhs.submission_index < rhs.submission_index;
                         });

        _stats.world_render_item_count = static_cast<std::uint32_t>(_world_render_items.items.size());
        build_world_indirect_batches(_world_indirect_batches);
        _stats.world_indirect_batch_count = static_cast<std::uint32_t>(_world_indirect_batches.size());
        ensure_world_indirect_quad_buffers();

        if (_world_item_cull_pipeline &&
            _world_indirect_quad_vertex_buffer &&
            _world_indirect_quad_index_buffer)
        {
            for (std::size_t batch_index{ 0u }; batch_index < _world_indirect_batches.size(); ++batch_index)
            {
                const world_indirect_batch_t& batch{ _world_indirect_batches[batch_index] };
                if (!batch.texture || batch.item_count == 0u)
                    continue;

                ensure_world_item_cull_gpu_buffers(batch_index, batch.item_count);
                const gpu_world_item_cull_constants_t cull_constants{
                    .visible_bounds_px = {
                        _active_camera.position.x,
                        _active_camera.position.y,
                        _active_camera.position.x + resolved_world_camera.visible_world_size.x,
                        _active_camera.position.y + resolved_world_camera.visible_world_size.y
                    },
                    .counts = {
                        static_cast<std::uint32_t>(batch.item_count),
                        0u,
                        0u,
                        0u
                    }
                };
                upload_world_item_cull_input(batch_index,
                                             std::span<const world_render_item_t>{
                                                 _world_render_items.items.data() + batch.first_item,
                                                 batch.item_count
                                             },
                                             cull_constants);
            }
            for (std::size_t batch_index{ 0u }; batch_index < _world_indirect_batches.size(); ++batch_index)
            {
                const world_indirect_batch_t& batch{ _world_indirect_batches[batch_index] };
                if (!batch.texture || batch.item_count == 0u)
                    continue;

                const world_item_cull_gpu_buffers_t& cull_buffers{ current_world_item_cull_gpu_buffers()[batch_index] };
                const std::array<rhi::compute_buffer_binding_t, 2> cull_read_only_bindings{
                    rhi::compute_buffer_binding_t{ .slot = 0u, .buffer = cull_buffers.constants_buffer.get() },
                    rhi::compute_buffer_binding_t{ .slot = 1u, .buffer = cull_buffers.item_input_buffer.get() }
                };
                const std::array<rhi::compute_buffer_binding_t, 2> cull_storage_bindings{
                    rhi::compute_buffer_binding_t{ .slot = 2u, .buffer = cull_buffers.visible_item_index_buffer.get() },
                    rhi::compute_buffer_binding_t{ .slot = 3u, .buffer = cull_buffers.output_buffer.get() }
                };

                _rhi->dispatch_compute({
                    .pipeline = _world_item_cull_pipeline.get(),
                    .read_only_buffers = cull_read_only_bindings,
                    .storage_buffers = cull_storage_bindings,
                    .graphics_handoff = rhi::compute_graphics_handoff_t::storage_write_to_graphics_read,
                    .group_count_x = 1u,
                    .group_count_y = 1u,
                    .group_count_z = 1u
                });
            }

            for (std::size_t batch_index{ 0u }; batch_index < _world_indirect_batches.size(); ++batch_index)
            {
                const world_indirect_batch_t& batch{ _world_indirect_batches[batch_index] };
                if (!batch.texture || batch.item_count == 0u)
                    continue;

                const world_item_cull_gpu_buffers_t& cull_buffers{ current_world_item_cull_gpu_buffers()[batch_index] };
                rhi::rhi_sampler_t* sampler{
                    _rhi->get_or_create_sampler(rhi::sampler_desc_from_preset(batch.sampler_preset))
                };
                if (!sampler)
                {
                    LOG_GRAPHICS_FATAL("Failed to resolve world indirect sampler");
                    continue;
                }

                _rhi->record_indirect_textured_quad_stage({
                    .vertex_buffer = _world_indirect_quad_vertex_buffer.get(),
                    .index_buffer = _world_indirect_quad_index_buffer.get(),
                    .indirect_buffer = cull_buffers.output_buffer.get(),
                    .texture = batch.texture,
                    .sampler = sampler,
                    .view_projection = stage_context.view_projection,
                    .ambient_color = _world_ambient_color,
                    .forward_plus_constants = _world_forward_plus_constants,
                    .forward_plus_light_input = _world_forward_plus_light_input,
                    .forward_plus_output = _world_forward_plus_output,
                    .forward_plus_light_input_buffer = gpu_buffers.light_input_buffer.get(),
                    .forward_plus_output_buffer = gpu_buffers.classification_output_buffer.get(),
                    .world_item_buffer = cull_buffers.item_input_buffer.get(),
                    .visible_item_index_buffer = cull_buffers.visible_item_index_buffer.get(),
                    .world_draw_mode = 1u,
                    .viewport = stage_context.viewport,
                    .indirect_buffer_offset_bytes = sizeof(gpu_world_item_cull_state_t),
                    .presentation_mask = world_stage_plan.presentation_mask
                });

                ++_stats.draw_calls;
                ++_stats.textured_quad_batch_count;
            }
        }

        const rhi::textured_quad_stage_record_t world_text_record{
            .view_projection = stage_context.view_projection,
            .ambient_color = { 1.f, 1.f, 1.f, 1.f },
            .forward_plus_constants = { },
            .forward_plus_light_input = { },
            .forward_plus_output = { },
            .forward_plus_light_input_buffer = gpu_buffers.light_input_buffer.get(),
            .forward_plus_output_buffer = gpu_buffers.classification_output_buffer.get(),
            .viewport = stage_context.viewport,
            .presentation_mask = world_stage_plan.presentation_mask
        };

        record_stage_state(*group.text, world_text_record, true);
    }

    void renderer_t::execute_frame_stage(const frame_stage_plan_t& stage_plan)
    {
        if (stage_plan.kind == frame_stage_kind_t::composite)
            materialize_composite_fullscreen_passes();

        const stage_execution_context_t stage_context{ resolve_stage_execution_context(stage_plan) };
        const stage_submission_group_t group{ stage_submission_group(stage_plan.kind) };
        CE_ASSERT(group.textured != nullptr && group.text != nullptr,
                  "Renderer non-world stage must have both textured and text submission state");

        const rhi::textured_quad_stage_record_t record{
            .view_projection = stage_context.view_projection,
            .ambient_color = { 1.f, 1.f, 1.f, 1.f },
            .forward_plus_constants = { },
            .forward_plus_light_input = { },
            .forward_plus_output = { },
            .forward_plus_light_input_buffer = current_forward_plus_gpu_buffers().light_input_buffer.get(),
            .forward_plus_output_buffer = current_forward_plus_gpu_buffers().classification_output_buffer.get(),
            .viewport = stage_context.viewport,
            .presentation_mask = stage_plan.presentation_mask
        };

        record_stage_state(*group.textured, record, false);
        record_stage_state(*group.text, record, true);

        if (stage_plan.kind == frame_stage_kind_t::composite)
            record_transition_battle_swirl_if_needed(stage_context, stage_plan.presentation_mask);
    }

    void renderer_t::execute_frame_stages()
    {
        validate_frame_stage_plan();

        _stats.vertex_count = 0;
        _stats.index_count = 0;
        _stats.textured_quad_batch_count = 0;
        _stats.draw_calls = 0;

        for (const frame_stage_plan_t& stage_plan : _frame_stage_plan)
        {
            if (_transition_battle_swirl.enabled && stage_plan.kind == frame_stage_kind_t::overlay_debug)
                continue;

            if (stage_plan.kind == frame_stage_kind_t::world)
                execute_world_frame_stage();
            else
                execute_frame_stage(stage_plan);
        }
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

        for (const frame_stage_plan_t& stage_plan : _frame_stage_plan)
        {
            const stage_submission_group_t group{ stage_submission_group(stage_plan.kind) };
            if (group.textured)
                release_stage_buffers(*group.textured);
            if (group.text)
                release_stage_buffers(*group.text);
        }

        for (forward_plus_gpu_buffers_t& frame_buffers : _forward_plus_gpu_buffers)
        {
            frame_buffers.constants_buffer.reset();
            frame_buffers.light_input_buffer.reset();
            frame_buffers.classification_output_buffer.reset();
        }

        for (auto& frame_buffer_set : _world_item_cull_gpu_buffers)
        {
            for (world_item_cull_gpu_buffers_t& frame_buffers : frame_buffer_set)
            {
                frame_buffers.constants_buffer.reset();
                frame_buffers.item_input_buffer.reset();
                frame_buffers.visible_item_index_buffer.reset();
                frame_buffers.output_buffer.reset();
                frame_buffers.item_capacity = 0u;
            }

            frame_buffer_set.clear();
        }

        _world_render_items.items.clear();
        _world_indirect_batches.clear();
        _composite_fullscreen_passes.clear();
        _world_indirect_quad_vertex_buffer.reset();
        _world_indirect_quad_index_buffer.reset();
        _transition_battle_swirl_vertex_buffer.reset();
        _transition_battle_swirl_index_buffer.reset();
        _transition_battle_swirl_capture_texture.reset();

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

    void renderer_t::ensure_forward_plus_gpu_buffers()
    {
        if (!_rhi)
            return;

        constexpr size_t constants_size_bytes{ sizeof(forward_plus_frame_constants_t) };
        constexpr size_t light_input_size_bytes{ sizeof(forward_plus_light_input_t) };
        constexpr size_t classification_output_size_bytes{ sizeof(forward_plus_classification_output_t) };

        for (forward_plus_gpu_buffers_t& frame_buffers : _forward_plus_gpu_buffers)
        {
            if (!frame_buffers.constants_buffer)
            {
                frame_buffers.constants_buffer = _rhi->create_buffer({
                    .size_bytes = constants_size_bytes,
                    .usage = rhi::buffer_usage_t::shader_read,
                    .cpu_writable = true
                });
            }

            if (!frame_buffers.light_input_buffer)
            {
                frame_buffers.light_input_buffer = _rhi->create_buffer({
                    .size_bytes = light_input_size_bytes,
                    .usage = rhi::buffer_usage_t::shader_read,
                    .cpu_writable = true
                });
            }

            if (!frame_buffers.classification_output_buffer)
            {
                frame_buffers.classification_output_buffer = _rhi->create_buffer({
                    .size_bytes = classification_output_size_bytes,
                    .usage = rhi::buffer_usage_t::storage
                });
            }

            if (!frame_buffers.constants_buffer ||
                !frame_buffers.light_input_buffer ||
                !frame_buffers.classification_output_buffer)
            {
                LOG_GRAPHICS_FATAL("Failed to create renderer forward+ GPU buffers");
                return;
            }
        }
    }

    void renderer_t::upload_forward_plus_gpu_data() const
    {
        const forward_plus_gpu_buffers_t& frame_buffers{ current_forward_plus_gpu_buffers() };

        if (!frame_buffers.constants_buffer ||
            !frame_buffers.light_input_buffer ||
            !frame_buffers.classification_output_buffer)
        {
            LOG_GRAPHICS_FATAL("Forward+ GPU buffers are not available for upload");
            return;
        }

        if (!frame_buffers.constants_buffer->write(&_world_forward_plus_constants, sizeof(_world_forward_plus_constants), 0u))
        {
            LOG_GRAPHICS_FATAL("Failed to upload forward+ frame constants");
            return;
        }

        if (!frame_buffers.light_input_buffer->write(&_world_forward_plus_light_input, sizeof(_world_forward_plus_light_input), 0u))
        {
            LOG_GRAPHICS_FATAL("Failed to upload forward+ light input");
            return;
        }
    }

    void renderer_t::ensure_world_item_cull_gpu_buffers(const std::size_t batch_index, const std::size_t item_capacity)
    {
        std::vector<world_item_cull_gpu_buffers_t>& frame_buffer_set{ current_world_item_cull_gpu_buffers() };
        if (frame_buffer_set.size() <= batch_index)
            frame_buffer_set.resize(batch_index + 1u);

        world_item_cull_gpu_buffers_t& frame_buffers{ frame_buffer_set[batch_index] };
        const std::size_t resolved_capacity{ std::max<std::size_t>(1u, item_capacity) };

        if (frame_buffers.item_capacity >= resolved_capacity &&
            frame_buffers.constants_buffer &&
            frame_buffers.item_input_buffer &&
            frame_buffers.visible_item_index_buffer &&
            frame_buffers.output_buffer)
        {
            return;
        }

        frame_buffers.item_capacity = resolved_capacity;
        frame_buffers.constants_buffer = _rhi->create_buffer({
            .size_bytes = sizeof(gpu_world_item_cull_constants_t),
            .usage = rhi::buffer_usage_t::shader_read,
            .cpu_writable = true
        });
        frame_buffers.item_input_buffer = _rhi->create_buffer({
            .size_bytes = resolved_capacity * sizeof(gpu_world_render_item_t),
            .usage = rhi::buffer_usage_t::shader_read,
            .cpu_writable = true
        });
        frame_buffers.visible_item_index_buffer = _rhi->create_buffer({
            .size_bytes = resolved_capacity * sizeof(std::uint32_t),
            .usage = rhi::buffer_usage_t::storage
        });
        frame_buffers.output_buffer = _rhi->create_buffer({
            .size_bytes = sizeof(gpu_world_item_cull_state_t) + sizeof(rhi::indexed_indirect_draw_command_t),
            .usage = rhi::buffer_usage_t::indirect
        });

        if (!frame_buffers.constants_buffer ||
            !frame_buffers.item_input_buffer ||
            !frame_buffers.visible_item_index_buffer ||
            !frame_buffers.output_buffer)
        {
            LOG_GRAPHICS_FATAL("Failed to create renderer world item cull GPU buffers");
        }
    }

    void renderer_t::upload_world_item_cull_input(const std::size_t batch_index,
                                                  const std::span<const world_render_item_t> items,
                                                  const gpu_world_item_cull_constants_t& cull_constants) const
    {
        const std::vector<world_item_cull_gpu_buffers_t>& frame_buffer_set{ current_world_item_cull_gpu_buffers() };
        if (batch_index >= frame_buffer_set.size())
        {
            LOG_GRAPHICS_FATAL("World item cull GPU buffers are not available for batch {}", batch_index);
            return;
        }

        const world_item_cull_gpu_buffers_t& frame_buffers{ frame_buffer_set[batch_index] };
        if (!frame_buffers.constants_buffer ||
            !frame_buffers.item_input_buffer ||
            !frame_buffers.visible_item_index_buffer ||
            !frame_buffers.output_buffer)
        {
            LOG_GRAPHICS_FATAL("World item cull GPU buffers are not available for upload");
            return;
        }

        std::vector<gpu_world_render_item_t> gpu_items;
        gpu_items.reserve(items.size());
        for (const world_render_item_t& item : items)
        {
            gpu_items.push_back({
                .quad_rect_px = {
                    item.position_px.x,
                    item.position_px.y,
                    item.size_px.x,
                    item.size_px.y
                },
                .uv_rect = {
                    item.uv_rect.u_min,
                    item.uv_rect.v_min,
                    item.uv_rect.u_max,
                    item.uv_rect.v_max
                },
                .bounds_min_max_px = {
                    item.bounds_min_px.x,
                    item.bounds_min_px.y,
                    item.bounds_max_px.x,
                    item.bounds_max_px.y
                },
                .packed_data = {
                    item.color,
                    0u,
                    0u,
                    0u
                },
                .draw_params = {
                    item.effect_mode,
                    item.effect_param0,
                    0.f,
                    0.f
                }
            });
        }

        if (!gpu_items.empty() &&
            !frame_buffers.item_input_buffer->write(gpu_items.data(), gpu_items.size() * sizeof(gpu_world_render_item_t), 0u))
        {
            LOG_GRAPHICS_FATAL("Failed to upload world item cull input buffer");
        }

        if (!frame_buffers.constants_buffer->write(&cull_constants, sizeof(cull_constants), 0u))
        {
            LOG_GRAPHICS_FATAL("Failed to upload world item cull constants buffer");
        }
    }

    void renderer_t::ensure_world_indirect_quad_buffers()
    {
        if (!_rhi || (_world_indirect_quad_vertex_buffer && _world_indirect_quad_index_buffer))
            return;

        constexpr std::array<quad_vertex_t, 4> quad_vertices{
            quad_vertex_t{ .x = 0.f, .y = 0.f, .u = 0.f, .v = 0.f, .color = 0xFFFFFFFFu, .effect_mode = 0.f, .effect_param0 = 0.f },
            quad_vertex_t{ .x = 1.f, .y = 0.f, .u = 1.f, .v = 0.f, .color = 0xFFFFFFFFu, .effect_mode = 0.f, .effect_param0 = 0.f },
            quad_vertex_t{ .x = 1.f, .y = 1.f, .u = 1.f, .v = 1.f, .color = 0xFFFFFFFFu, .effect_mode = 0.f, .effect_param0 = 0.f },
            quad_vertex_t{ .x = 0.f, .y = 1.f, .u = 0.f, .v = 1.f, .color = 0xFFFFFFFFu, .effect_mode = 0.f, .effect_param0 = 0.f }
        };
        constexpr std::array<std::uint32_t, 6> quad_indices{ 0u, 1u, 2u, 0u, 2u, 3u };

        _world_indirect_quad_vertex_buffer = _rhi->create_buffer({
            .size_bytes = quad_vertices.size() * sizeof(quad_vertex_t),
            .usage = rhi::buffer_usage_t::vertex,
            .initial_data = quad_vertices.data()
        });
        _world_indirect_quad_index_buffer = _rhi->create_buffer({
            .size_bytes = quad_indices.size() * sizeof(std::uint32_t),
            .usage = rhi::buffer_usage_t::index,
            .initial_data = quad_indices.data()
        });

        if (!_world_indirect_quad_vertex_buffer || !_world_indirect_quad_index_buffer)
            LOG_GRAPHICS_FATAL("Failed to create renderer world indirect quad buffers");
    }

    void renderer_t::ensure_transition_battle_swirl_capture_texture()
    {
        if (!_rhi)
            return;

        const chlm::uint2 target_size{ current_render_target_size() };
        if (target_size.x == 0u || target_size.y == 0u)
            return;

        if (_transition_battle_swirl_capture_texture &&
            _transition_battle_swirl_capture_texture->width() == target_size.x &&
            _transition_battle_swirl_capture_texture->height() == target_size.y)
        {
            return;
        }

        _transition_battle_swirl_capture_texture = _rhi->create_texture_2d({
            .width = target_size.x,
            .height = target_size.y,
            .format = rhi::texture_format_t::rgba8_srgb
        });

        if (!_transition_battle_swirl_capture_texture)
            LOG_GRAPHICS_FATAL("Failed to create transition battle swirl capture texture");
    }

    void renderer_t::ensure_transition_battle_swirl_quad_buffers()
    {
        if (!_rhi ||
            (_transition_battle_swirl_vertex_buffer && _transition_battle_swirl_index_buffer))
        {
            return;
        }

        constexpr std::array<quad_vertex_t, 4> quad_vertices{
            quad_vertex_t{ .x = 0.f, .y = 0.f, .u = 0.f, .v = 0.f, .color = 0xFFFFFFFFu, .effect_mode = 0.f, .effect_param0 = 0.f },
            quad_vertex_t{ .x = 1.f, .y = 0.f, .u = 1.f, .v = 0.f, .color = 0xFFFFFFFFu, .effect_mode = 0.f, .effect_param0 = 0.f },
            quad_vertex_t{ .x = 1.f, .y = 1.f, .u = 1.f, .v = 1.f, .color = 0xFFFFFFFFu, .effect_mode = 0.f, .effect_param0 = 0.f },
            quad_vertex_t{ .x = 0.f, .y = 1.f, .u = 0.f, .v = 1.f, .color = 0xFFFFFFFFu, .effect_mode = 0.f, .effect_param0 = 0.f }
        };
        constexpr std::array<std::uint32_t, 6> quad_indices{ 0u, 1u, 2u, 0u, 2u, 3u };

        _transition_battle_swirl_vertex_buffer = _rhi->create_buffer({
            .size_bytes = quad_vertices.size() * sizeof(quad_vertex_t),
            .usage = rhi::buffer_usage_t::vertex,
            .cpu_writable = true,
            .initial_data = quad_vertices.data()
        });
        _transition_battle_swirl_index_buffer = _rhi->create_buffer({
            .size_bytes = quad_indices.size() * sizeof(std::uint32_t),
            .usage = rhi::buffer_usage_t::index,
            .cpu_writable = true,
            .initial_data = quad_indices.data()
        });

        if (!_transition_battle_swirl_vertex_buffer || !_transition_battle_swirl_index_buffer)
            LOG_GRAPHICS_FATAL("Failed to create renderer transition battle swirl quad buffers");
    }

    void renderer_t::build_world_indirect_batches(std::vector<world_indirect_batch_t>& out_batches) const
    {
        out_batches.clear();
        if (_world_render_items.items.empty())
            return;

        std::size_t batch_first_item{ 0u };
        const auto batch_matches = [](const world_render_item_t& lhs, const world_render_item_t& rhs) noexcept
        {
            return lhs.texture == rhs.texture &&
                   lhs.sampler_preset == rhs.sampler_preset &&
                   lhs.world_material == rhs.world_material;
        };

        for (std::size_t item_index{ 1u }; item_index <= _world_render_items.items.size(); ++item_index)
        {
            const bool flush_batch{
                item_index == _world_render_items.items.size() ||
                !batch_matches(_world_render_items.items[batch_first_item], _world_render_items.items[item_index])
            };
            if (!flush_batch)
                continue;

            const world_render_item_t& batch_head{ _world_render_items.items[batch_first_item] };
            out_batches.push_back({
                .texture = batch_head.texture,
                .sampler_preset = batch_head.sampler_preset,
                .world_material = batch_head.world_material,
                .first_item = batch_first_item,
                .item_count = item_index - batch_first_item
            });
            batch_first_item = item_index;
        }
    }

    void renderer_t::update_forward_plus_diagnostics() noexcept
    {
        _stats.forward_plus_light_index_count = 0u;
        _stats.forward_plus_dropped_light_references = 0u;

        const std::uint32_t tile_count_x{ _world_forward_plus_constants.tile_counts[0] };
        const std::uint32_t tile_count_y{ _world_forward_plus_constants.tile_counts[1] };
        if (tile_count_x == 0u || tile_count_y == 0u)
            return;

        const float tile_size{ std::max(_world_forward_plus_constants.grid_params.z, 1.0f) };
        const chlm::float2 grid_origin{
            _world_forward_plus_constants.grid_params.x,
            _world_forward_plus_constants.grid_params.y
        };
        const std::uint32_t point_light_count{
            std::min(_world_forward_plus_constants.point_light_counts[0],
                     static_cast<std::uint32_t>(k_max_world_point_lights))
        };
        constexpr std::uint32_t per_tile_light_budget{ static_cast<std::uint32_t>(k_max_world_point_lights) };

        // This mirrors the compute overlap math only for renderer diagnostics.
        // The GPU path still owns the live tile/light list used by rendering.
        for (std::uint32_t tile_y{ 0u }; tile_y < tile_count_y; ++tile_y)
        {
            for (std::uint32_t tile_x{ 0u }; tile_x < tile_count_x; ++tile_x)
            {
                const chlm::float2 tile_min{
                    grid_origin.x + (static_cast<float>(tile_x) * tile_size),
                    grid_origin.y + (static_cast<float>(tile_y) * tile_size)
                };
                const chlm::float2 tile_max{
                    tile_min.x + tile_size,
                    tile_min.y + tile_size
                };

                std::uint32_t tile_light_count{ 0u };
                for (std::uint32_t light_index{ 0u }; light_index < point_light_count; ++light_index)
                {
                    const world_point_light_uniform_t& light{ _world_forward_plus_light_input.point_lights[light_index] };
                    if (!circle_overlaps_aabb(chlm::float2{ light.position_radius_px.x, light.position_radius_px.y },
                                              light.position_radius_px.z,
                                              tile_min,
                                              tile_max))
                    {
                        continue;
                    }

                    if (tile_light_count < per_tile_light_budget)
                    {
                        ++tile_light_count;
                    }
                    else
                    {
                        ++_stats.forward_plus_dropped_light_references;
                    }
                }

                _stats.forward_plus_light_index_count += tile_light_count;
            }
        }
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

    forward_plus_gpu_buffers_t& renderer_t::current_forward_plus_gpu_buffers() noexcept
    {
        return _forward_plus_gpu_buffers[current_textured_quad_frame_buffer_slot()];
    }

    const forward_plus_gpu_buffers_t& renderer_t::current_forward_plus_gpu_buffers() const noexcept
    {
        return _forward_plus_gpu_buffers[current_textured_quad_frame_buffer_slot()];
    }

    std::vector<world_item_cull_gpu_buffers_t>& renderer_t::current_world_item_cull_gpu_buffers() noexcept
    {
        return _world_item_cull_gpu_buffers[current_textured_quad_frame_buffer_slot()];
    }

    const std::vector<world_item_cull_gpu_buffers_t>& renderer_t::current_world_item_cull_gpu_buffers() const noexcept
    {
        return _world_item_cull_gpu_buffers[current_textured_quad_frame_buffer_slot()];
    }
} // namespace carrot::renderer
