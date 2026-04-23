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

        [[nodiscard]] textured_quad_draw_info_t quad_sort_draw_info(const quad_instance_t& quad) noexcept
        {
            textured_quad_draw_info_t sort_info{ };
            sort_info.layer = quad.layer;
            sort_info.order_mode = quad.order_mode;
            sort_info.order_in_layer = quad.order_in_layer;
            sort_info.sort_reference_y = quad.sort_reference_y;
            return sort_info;
        }

        [[nodiscard]] bool quad_instance_sorts_before(const quad_instance_t& lhs, const quad_instance_t& rhs) noexcept
        {
            const textured_quad_draw_info_t lhs_quad{ quad_sort_draw_info(lhs) };
            const textured_quad_draw_info_t rhs_quad{ quad_sort_draw_info(rhs) };

            if (quad_sorts_before(lhs_quad, rhs_quad))
                return true;

            if (quad_sorts_before(rhs_quad, lhs_quad))
                return false;

            return lhs.submission_index < rhs.submission_index;
        }

        [[nodiscard]] quad_bucket_key_t quad_bucket_key(const quad_instance_t& quad) noexcept
        {
            return quad_bucket_key_t{
                .stage = quad.stage,
                .target_id = quad.target_id,
                .content_kind = quad.content_kind,
                .texture = quad.texture,
                .sampler_preset = quad.sampler_preset,
                .world_material = quad.world_material
            };
        }

        [[nodiscard]] quad_target_id_t quad_target_for_composite_target(const composite_target_kind_t target_kind) noexcept
        {
            return quad_target_id_t{
                .target = static_cast<std::uint16_t>(target_kind),
                .pass = 0u
            };
        }

        [[nodiscard]] chlm::float4 unpack_abgr_color_to_float4(const std::uint32_t packed_color) noexcept
        {
            return {
                static_cast<float>((packed_color >> 0u) & 0xFFu) / 255.0f,
                static_cast<float>((packed_color >> 8u) & 0xFFu) / 255.0f,
                static_cast<float>((packed_color >> 16u) & 0xFFu) / 255.0f,
                static_cast<float>((packed_color >> 24u) & 0xFFu) / 255.0f
            };
        }

        [[nodiscard]] std::optional<textured_quad_draw_info_t> sprite_draw_info_to_quad(
            const sprite_draw_info_t& info) noexcept
        {
            if (!info.sprite || !info.frame)
                return std::nullopt;

            const assets::loaded_texture_asset_t* texture_asset{ info.sprite->texture() };
            if (!texture_asset || !texture_asset->texture)
                return std::nullopt;

            const float texture_width{ static_cast<float>(texture_asset->texture->width()) };
            const float texture_height{ static_cast<float>(texture_asset->texture->height()) };
            if (texture_width <= 0.f || texture_height <= 0.f)
                return std::nullopt;

            const chlm::uint_rect& rect{ info.frame->pixel_rect };
            const float u0{ static_cast<float>(rect.position.x) / texture_width };
            const float v0{ static_cast<float>(rect.position.y) / texture_height };
            const float u1{ static_cast<float>(rect.position.x + rect.size.x) / texture_width };
            const float v1{ static_cast<float>(rect.position.y + rect.size.y) / texture_height };
            const chlm::float2 pivot{ info.use_custom_pivot ? info.pivot : info.frame->pivot };

            textured_quad_draw_info_t quad{ };
            quad.texture = texture_asset->texture.get();
            quad.x = info.x - (pivot.x * info.width);
            quad.y = info.y - (pivot.y * info.height);
            quad.width = info.width;
            quad.height = info.height;
            quad.u0 = info.flip_x ? u1 : u0;
            quad.v0 = info.flip_y ? v1 : v0;
            quad.u1 = info.flip_x ? u0 : u1;
            quad.v1 = info.flip_y ? v0 : v1;
            quad.layer = info.layer;
            quad.order_mode = info.order_mode;
            quad.order_in_layer = info.order_in_layer;
            quad.sort_reference_y = info.order_mode == render_order_mode_t::anchor_bottom_y
                                        ? (quad.y + quad.height)
                                        : info.sort_reference_y;
            quad.color = info.color;
            quad.sampler_preset = info.sampler_preset;
            return quad;
        }

        [[nodiscard]] bool aabb_overlaps_aabb(const chlm::float2 a_min,
                                              const chlm::float2 a_max,
                                              const chlm::float2 b_min,
                                              const chlm::float2 b_max) noexcept
        {
            return a_max.x >= b_min.x &&
                   a_max.y >= b_min.y &&
                   a_min.x <= b_max.x &&
                   a_min.y <= b_max.y;
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
        desc.present_sync_enabled = _config.present_sync_enabled;
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
        ensure_shared_quad_geometry_buffers();

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
        _shared_quad_vertex_buffer.reset();
        _shared_quad_index_buffer.reset();
        _forward_plus_classify_pipeline.reset();
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
        _transition_wipe = { };
        _transition_battle_swirl = { };
        _composite_overlay_enabled = false;
        _composite_overlay_color = 0x00000000u;
        _bloom_source_passes.clear();
        _post_effect_passes.clear();
        _transition_passes.clear();
        _bloom_source_quads.instances.clear();
        _bloom_blur_horizontal_quads.instances.clear();
        _bloom_blur_vertical_quads.instances.clear();
        _bloom_composite_quads.instances.clear();
        _world_ambient_color = { 1.f, 1.f, 1.f, 1.f };
        _world_quad_instances.instances.clear();
        _world_forward_plus_light_input = { };
        _world_forward_plus_constants = { };
        _world_forward_plus_output = { };
        refresh_composite_targets();

        _rhi->begin_frame();
    }

    void renderer_t::end_frame()
    {
        queue_bloom_if_needed();
        compose_bloom_sources();
        queue_transition_fade_if_needed();
        queue_transition_wipe_if_needed();
        queue_transition_battle_swirl_if_needed();
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

    void renderer_t::draw_bloom_textured_quad(const textured_quad_draw_info_t& quad)
    {
        queue_bloom_source_textured_pass(composite_target_kind_t::gameplay_present,
                                         quad,
                                         "bloom_textured_quad");
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

    void renderer_t::draw_bloom_solid_quad(const solid_quad_draw_info_t& quad)
    {
        queue_bloom_source_solid_pass(composite_target_kind_t::gameplay_present,
                                      quad,
                                      "bloom_solid_quad");
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

    void renderer_t::set_transition_wipe(const float coverage,
                                         const transition_wipe_direction_t direction,
                                         const uint32_t color_abgr) noexcept
    {
        _transition_wipe.enabled = true;
        _transition_wipe.coverage = std::clamp(coverage, 0.f, 1.f);
        _transition_wipe.direction = direction;
        _transition_wipe.color_abgr = color_abgr;
    }

    void renderer_t::clear_transition_wipe() noexcept
    {
        _transition_wipe = { };
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

    light_shaft_readiness_t renderer_t::light_shaft_readiness() const noexcept
    {
        return light_shaft_readiness_t{
            .renderer_contract_ready = true,
            .composite_capture_source_available = true,
            .fullscreen_pass_orchestration_available = true,
            .point_light_source_input_available = true,
            .requires_world_occlusion_mask = true,
            .requires_source_mask_texture = true,
            .requires_authored_shaft_source_selection = true
        };
    }

    quad_instance_t renderer_t::build_quad_instance(const frame_stage_plan_t& stage_plan,
                                                    const quad_content_kind_t content_kind,
                                                    const textured_quad_draw_info_t& quad,
                                                    const world_material_key_t world_material,
                                                    const quad_target_id_t target_id) const
    {
        return quad_instance_t{
            .stage = stage_plan.kind,
            .stage_space = stage_plan.space,
            .target_id = target_id,
            .content_kind = content_kind,
            .texture = quad.texture,
            .sampler_preset = quad.sampler_preset,
            .world_material = world_material,
            .x = quad.x,
            .y = quad.y,
            .width = quad.width,
            .height = quad.height,
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
            .bounds_min_px = { quad.x, quad.y },
            .bounds_max_px = { quad.x + quad.width, quad.y + quad.height },
            .presentation_mask = stage_plan.presentation_mask,
            .submission_index = 0u
        };
    }

    rhi::quad_stage_common_t renderer_t::build_quad_stage_common_record(const stage_execution_context_t& stage_context,
                                                                        const std::uint32_t presentation_mask,
                                                                        const forward_plus_gpu_buffers_t* const gpu_buffers) const
    {
        return rhi::quad_stage_common_t{
            .view_projection = stage_context.view_projection,
            .ambient_color = { 1.f, 1.f, 1.f, 1.f },
            .forward_plus_constants = { },
            .forward_plus_light_input = { },
            .forward_plus_output = { },
            .forward_plus_light_input_buffer = gpu_buffers ? gpu_buffers->light_input_buffer.get() : nullptr,
            .forward_plus_output_buffer = gpu_buffers ? gpu_buffers->classification_output_buffer.get() : nullptr,
            .world_item_buffer = nullptr,
            .visible_item_index_buffer = nullptr,
            .world_draw_mode = 0u,
            .viewport = stage_context.viewport,
            .presentation_mask = presentation_mask
        };
    }

    rhi::quad_draw_source_t renderer_t::build_quad_draw_source_record(const rhi::rhi_buffer_t* const vertex_buffer,
                                                                      const rhi::rhi_buffer_t* const index_buffer,
                                                                      const rhi::rhi_buffer_t* const instance_buffer,
                                                                      const rhi::rhi_buffer_t* const indirect_buffer,
                                                                      const std::uint32_t instance_count,
                                                                      const std::uint32_t instance_buffer_offset_bytes,
                                                                      const std::uint32_t indirect_buffer_offset_bytes) const
    {
        return rhi::quad_draw_source_t{
            .vertex_buffer = vertex_buffer,
            .index_buffer = index_buffer,
            .instance_buffer = instance_buffer,
            .indirect_buffer = indirect_buffer,
            .instance_count = instance_count,
            .instance_buffer_offset_bytes = instance_buffer_offset_bytes,
            .indirect_buffer_offset_bytes = indirect_buffer_offset_bytes,
            .kind = indirect_buffer != nullptr
                        ? rhi::quad_draw_source_kind_t::indexed_indirect
                        : rhi::quad_draw_source_kind_t::direct
        };
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
        quad_instance_t instance{
            build_quad_instance(stage_plan(frame_stage_kind_t::world), quad_content_kind_t::textured, quad, world_material)
        };
        instance.submission_index = static_cast<std::uint64_t>(_world_quad_instances.instances.size());
        _world_quad_instances.instances.push_back(instance);
    }

    void renderer_t::submit_stage_textured_quad(const frame_stage_plan_t& stage_plan,
                                                const textured_quad_draw_info_t& quad,
                                                const quad_target_id_t target_id)
    {
        if (quad.texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_textured_quad called with null texture");
            return;
        }

        CE_ASSERT(stage_plan.kind != frame_stage_kind_t::world,
                  "Renderer non-world textured quad submission must not target the world stage");

        textured_quad_state_t& stage_state{ _stage_textured_quads[frame_stage_index(stage_plan.kind)] };
        quad_instance_t instance{
            build_quad_instance(stage_plan,
                                quad_content_kind_t::textured,
                                quad,
                                {
                .domain = world_material_domain_t::unlit,
                .feature_flags = 0u
            },
                                target_id)
        };
        instance.submission_index = static_cast<uint64_t>(stage_state.instances.size());
        stage_state.instances.push_back(instance);

        _stats.textured_quad_count++;
    }

    void renderer_t::submit_non_world_textured_quad(const non_world_stage_target_t target,
                                                    const textured_quad_draw_info_t& quad,
                                                    const quad_target_id_t target_id)
    {
        submit_stage_textured_quad(non_world_stage_plan(target), quad, target_id);
    }

    void renderer_t::submit_solid_quad(const frame_stage_kind_t stage,
                                       const solid_quad_draw_info_t& quad,
                                       const quad_target_id_t target_id)
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

        submit_stage_textured_quad(stage_plan(stage), textured_quad, target_id);
    }

    void renderer_t::submit_world_text_quad(const textured_quad_draw_info_t& quad)
    {
        if (quad.texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_text_quad called with null texture");
            return;
        }

        quad_instance_t instance{
            build_quad_instance(stage_plan(frame_stage_kind_t::world),
                                quad_content_kind_t::text,
                                quad,
                                {
                .domain = world_material_domain_t::unlit,
                .feature_flags = 0u
            })
        };
        instance.submission_index = static_cast<uint64_t>(_world_text_quads.instances.size());
        _world_text_quads.instances.push_back(instance);

        _stats.textured_quad_count++;
    }

    void renderer_t::submit_stage_text_quad(const frame_stage_plan_t& stage_plan,
                                            const textured_quad_draw_info_t& quad,
                                            const quad_target_id_t target_id)
    {
        if (quad.texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_text_quad called with null texture");
            return;
        }

        CE_ASSERT(stage_plan.kind != frame_stage_kind_t::world,
                  "Renderer non-world text quad submission must not target the world stage");

        textured_quad_state_t& stage_state{ _stage_text_quads[frame_stage_index(stage_plan.kind)] };
        quad_instance_t instance{
            build_quad_instance(stage_plan,
                                quad_content_kind_t::text,
                                quad,
                                {
                .domain = world_material_domain_t::unlit,
                .feature_flags = 0u
            },
                                target_id)
        };
        instance.submission_index = static_cast<uint64_t>(stage_state.instances.size());
        stage_state.instances.push_back(instance);

        _stats.textured_quad_count++;
    }

    void renderer_t::submit_non_world_text_quad(const non_world_stage_target_t target,
                                                const textured_quad_draw_info_t& quad,
                                                const quad_target_id_t target_id)
    {
        submit_stage_text_quad(non_world_stage_plan(target), quad, target_id);
    }

    void renderer_t::submit_non_world_solid_quad(const non_world_stage_target_t target,
                                                 const solid_quad_draw_info_t& quad,
                                                 const quad_target_id_t target_id)
    {
        submit_solid_quad(non_world_stage_plan(target).kind, quad, target_id);
    }

    void renderer_t::draw_sprite(const sprite_draw_info_t& info)
    {
        const auto quad{ sprite_draw_info_to_quad(info) };
        if (!quad)
        {
            LOG_GRAPHICS_WARN("renderer_t::draw_sprite could not resolve sprite draw info into a textured quad.");
            return;
        }

        draw_textured_quad(*quad);
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

                const chlm::float2 tile_object_size_px{
                    render_size_px.x * transform.scale.x,
                    render_size_px.y * transform.scale.y
                };
                submit_tile_object(*tile_object.tilemap,
                                   tile_object.gid,
                                   render_position_px,
                                   tile_object_size_px,
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
                .textured = &_world_textured_quads,
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

        queue_post_effect_textured_pass(post_effect_domain_t::overlay,
                                        composite_target_kind_t::gameplay_present,
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
                                        "composite_overlay",
                                        true);
    }

    void renderer_t::queue_transition_fade_if_needed()
    {
        if (!_transition_fade_enabled)
            return;

        _transition_passes.push_back(transition_pass_t{
            .kind = transition_pass_kind_t::fade,
            .target = composite_target_kind_t::gameplay_present,
            .color_abgr = _transition_fade_color,
            .debug_name = "transition_fade"
        });
    }

    void renderer_t::queue_transition_wipe_if_needed()
    {
        if (!_transition_wipe.enabled)
            return;

        _transition_passes.push_back(transition_pass_t{
            .kind = transition_pass_kind_t::wipe,
            .target = composite_target_kind_t::gameplay_present,
            .color_abgr = _transition_wipe.color_abgr,
            .coverage = _transition_wipe.coverage,
            .wipe_direction = static_cast<std::uint8_t>(_transition_wipe.direction),
            .debug_name = "transition_wipe"
        });
    }

    void renderer_t::queue_transition_battle_swirl_if_needed()
    {
        if (!_transition_battle_swirl.enabled)
            return;

        _transition_passes.push_back(transition_pass_t{
            .kind = transition_pass_kind_t::battle_swirl,
            .target = composite_target_kind_t::gameplay_present,
            .progress = _transition_battle_swirl.progress,
            .incoming = _transition_battle_swirl.incoming,
            .debug_name = "battle_swirl_transition"
        });
    }

    void renderer_t::record_transition_battle_swirl_pass(const transition_pass_t& pass,
                                                         const stage_execution_context_t& stage_context,
                                                         const uint32_t presentation_mask)
    {
        CE_ASSERT(pass.kind == transition_pass_kind_t::battle_swirl,
                  "Renderer battle swirl recorder received non-battle-swirl transition pass");

        ensure_shared_quad_geometry_buffers();
        ensure_transition_battle_swirl_capture_texture();

        if (!_rhi ||
            !_shared_quad_vertex_buffer ||
            !_shared_quad_index_buffer ||
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
        const float swirl_direction{ pass.incoming ? 1.f : -1.f };
        const float x0{ static_cast<float>(stage_context.viewport.rect_px.position.x) };
        const float y0{ static_cast<float>(stage_context.viewport.rect_px.position.y) };
        const float x1{ x0 + static_cast<float>(stage_context.viewport.rect_px.size.x) };
        const float y1{ y0 + static_cast<float>(stage_context.viewport.rect_px.size.y) };
        const gpu_quad_instance_t swirl_instance{
            .quad_rect_px = { x0, y0, x1 - x0, y1 - y0 },
            .uv_rect = { 0.f, 0.f, 1.f, 1.f },
            .color = { 1.f, 1.f, 1.f, 1.f },
            .draw_params = { swirl_direction, pass.progress, 0.f, 0.f }
        };

        const auto swirl_upload{
            _rhi->allocate_transient_vertex_upload(sizeof(gpu_quad_instance_t),
                                                   rhi::transient_upload::vertex_alignment<gpu_quad_instance_t>())
        };
        if (!swirl_upload || !swirl_upload->buffer || !swirl_upload->mapped_ptr)
        {
            LOG_GRAPHICS_FATAL("Failed to prepare transition battle swirl transient upload");
            return;
        }
        std::memcpy(swirl_upload->mapped_ptr, &swirl_instance, sizeof(gpu_quad_instance_t));

        const std::array<textured_quad_batch_t, 1u> swirl_batches{
            textured_quad_batch_t{
                .texture = _transition_battle_swirl_capture_texture.get(),
                .first_index = 0u,
                .index_count = static_cast<std::uint32_t>(quad_indices.size()),
                .first_instance = 0u,
                .instance_count = 1u,
                .sampler_preset = quad_sampler_preset_t::smooth_clamp,
                .world_material = { .domain = world_material_domain_t::unlit, .feature_flags = 0u }
            }
        };

        rhi::textured_quad_stage_record_t swirl_record{ };
        static_cast<rhi::quad_stage_common_t&>(swirl_record) = build_quad_stage_common_record(stage_context,
                                                                                               presentation_mask);
        static_cast<rhi::quad_draw_source_t&>(swirl_record) = build_quad_draw_source_record(
            _shared_quad_vertex_buffer.get(),
            _shared_quad_index_buffer.get(),
            swirl_upload->buffer,
            nullptr,
            1u,
            static_cast<std::uint32_t>(swirl_upload->offset_bytes));
        swirl_record.batches = swirl_batches;
        swirl_record.shader_variant = rhi::quad_shader_variant_t::battle_swirl;
        swirl_record.capture_presentation_before_draw = true;
        _rhi->record_textured_quad_stage(swirl_record);

        _stats.draw_calls++;
        _stats.textured_quad_batch_count++;
        _stats.vertex_count += 4u;
        _stats.index_count += 6u;
    }

    void renderer_t::materialize_transition_passes(const stage_execution_context_t& stage_context,
                                                   const uint32_t presentation_mask,
                                                   const transition_pass_phase_t phase)
    {
        for (const transition_pass_t& pass : _transition_passes)
        {
            switch (pass.kind)
            {
                case transition_pass_kind_t::fade:
                    if (phase == transition_pass_phase_t::pre_composite_record)
                        record_transition_fade_pass(pass);
                    break;
                case transition_pass_kind_t::wipe:
                    if (phase == transition_pass_phase_t::pre_composite_record)
                        record_transition_wipe_pass(pass);
                    break;
                case transition_pass_kind_t::battle_swirl:
                    if (phase == transition_pass_phase_t::post_composite_record)
                        record_transition_battle_swirl_pass(pass, stage_context, presentation_mask);
                    break;
            }
        }
    }

    void renderer_t::record_transition_fade_pass(const transition_pass_t& pass)
    {
        CE_ASSERT(pass.kind == transition_pass_kind_t::fade,
                  "Renderer transition fade recorder received non-fade transition pass");

        const composite_target_t target{ composite_target(pass.target) };
        submit_non_world_solid_quad(non_world_stage_target_t::composite,
                                    solid_quad_draw_info_t{
                                        .x = static_cast<float>(target.rect_px.position.x),
                                        .y = static_cast<float>(target.rect_px.position.y),
                                        .width = static_cast<float>(target.rect_px.size.x),
                                        .height = static_cast<float>(target.rect_px.size.y),
                                        .layer = render_layer_t::ui,
                                        .order_mode = render_order_mode_t::explicit_order,
                                        .order_in_layer = 0,
                                        .sort_reference_y = 0.f,
                                        .sampler_preset = quad_sampler_preset_t::pixel_clamp,
                                        .color = pass.color_abgr
                                    },
                                    quad_target_for_composite_target(pass.target));
    }

    void renderer_t::record_transition_wipe_pass(const transition_pass_t& pass)
    {
        CE_ASSERT(pass.kind == transition_pass_kind_t::wipe,
                  "Renderer transition wipe recorder received non-wipe transition pass");

        const composite_target_t target{ composite_target(pass.target) };
        const float coverage{ std::clamp(pass.coverage, 0.f, 1.f) };
        const float target_x{ static_cast<float>(target.rect_px.position.x) };
        const float target_y{ static_cast<float>(target.rect_px.position.y) };
        const float target_width{ static_cast<float>(target.rect_px.size.x) };
        const float target_height{ static_cast<float>(target.rect_px.size.y) };

        float quad_x{ target_x };
        float quad_y{ target_y };
        float quad_width{ 0.f };
        float quad_height{ 0.f };

        switch (static_cast<transition_wipe_direction_t>(pass.wipe_direction))
        {
            case transition_wipe_direction_t::left_to_right:
                quad_width = coverage * target_width;
                quad_height = target_height;
                break;
            case transition_wipe_direction_t::right_to_left:
                quad_width = coverage * target_width;
                quad_height = target_height;
                quad_x = target_x + (target_width - quad_width);
                break;
            case transition_wipe_direction_t::top_to_bottom:
                quad_width = target_width;
                quad_height = coverage * target_height;
                break;
            case transition_wipe_direction_t::bottom_to_top:
                quad_width = target_width;
                quad_height = coverage * target_height;
                quad_y = target_y + (target_height - quad_height);
                break;
        }

        if (quad_width <= 0.001f || quad_height <= 0.001f)
            return;

        submit_non_world_solid_quad(non_world_stage_target_t::composite,
                                    solid_quad_draw_info_t{
                                        .x = quad_x,
                                        .y = quad_y,
                                        .width = quad_width,
                                        .height = quad_height,
                                        .layer = render_layer_t::ui,
                                        .order_mode = render_order_mode_t::explicit_order,
                                        .order_in_layer = 0,
                                        .sort_reference_y = 0.f,
                                        .sampler_preset = quad_sampler_preset_t::pixel_clamp,
                                        .color = pass.color_abgr
                                    },
                                    quad_target_for_composite_target(pass.target));
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

        queue_bloom_source_solid_pass(composite_target_kind_t::gameplay_present,
                                      solid_quad_draw_info_t{
            .x = 0.0f,
            .y = 0.0f,
            .width = 1.0f,
            .height = 1.0f,
            .layer = render_layer_t::ui,
            .order_in_layer = 0,
            .color = bloom_color
        },
                                      "bloom_overlay_source",
                                      true);
    }

    void renderer_t::queue_bloom_source_textured_pass(const composite_target_kind_t target_kind,
                                                      textured_quad_draw_info_t quad,
                                                      const char* const debug_name,
                                                      const bool expand_to_target)
    {
        // M27 contract: bloom sources are normalized into composite-space quads.
        // This keeps the offscreen proof path honest, but it also means world-space
        // authored bloom is not solved by this queue yet.
        quad.layer = render_layer_t::ui;
        quad.order_mode = render_order_mode_t::explicit_order;
        quad.order_in_layer = 0;
        quad.sort_reference_y = 0.f;

        _bloom_source_passes.push_back(bloom_source_pass_t{
            .kind = post_effect_pass_kind_t::textured,
            .target = target_kind,
            .expand_to_target = expand_to_target,
            .textured_quad = quad,
            .solid_quad = { },
            .debug_name = debug_name
        });
    }

    void renderer_t::queue_bloom_source_solid_pass(const composite_target_kind_t target_kind,
                                                   solid_quad_draw_info_t quad,
                                                   const char* const debug_name,
                                                   const bool expand_to_target)
    {
        // M27 contract: bloom sources are normalized into composite-space quads.
        quad.layer = render_layer_t::ui;
        quad.order_mode = render_order_mode_t::explicit_order;
        quad.order_in_layer = 0;
        quad.sort_reference_y = 0.f;

        _bloom_source_passes.push_back(bloom_source_pass_t{
            .kind = post_effect_pass_kind_t::solid,
            .target = target_kind,
            .expand_to_target = expand_to_target,
            .textured_quad = { },
            .solid_quad = quad,
            .debug_name = debug_name
        });
    }

    void renderer_t::materialize_bloom_source_passes()
    {
        _bloom_source_quads.instances.clear();

        // TODO(M27 follow-up): world-authored/selective bloom needs its own
        // source-space contract. This materializer currently assumes every bloom
        // source can be interpreted as a composite-stage quad in render-target
        // pixels, which is correct for the harness and overlay proof path only.
        for (const bloom_source_pass_t& pass : _bloom_source_passes)
        {
            const composite_target_t target{ composite_target(pass.target) };
            switch (pass.kind)
            {
                case post_effect_pass_kind_t::textured:
                {
                    textured_quad_draw_info_t quad{ pass.textured_quad };
                    if (pass.expand_to_target)
                    {
                        quad.x = static_cast<float>(target.rect_px.position.x);
                        quad.y = static_cast<float>(target.rect_px.position.y);
                        quad.width = static_cast<float>(target.rect_px.size.x);
                        quad.height = static_cast<float>(target.rect_px.size.y);
                    }

                    quad.layer = render_layer_t::ui;
                    quad.order_mode = render_order_mode_t::explicit_order;
                    quad.order_in_layer = 0;
                    quad.sort_reference_y = 0.f;

                    quad_instance_t instance{
                        build_quad_instance(non_world_stage_plan(non_world_stage_target_t::composite),
                                            quad_content_kind_t::textured,
                                            quad,
                                            { .domain = world_material_domain_t::unlit, .feature_flags = 0u },
                                            quad_target_for_composite_target(pass.target))
                    };
                    instance.submission_index = static_cast<std::uint64_t>(_bloom_source_quads.instances.size());
                    _bloom_source_quads.instances.push_back(instance);
                    break;
                }

                case post_effect_pass_kind_t::solid:
                {
                    if (_solid_white_texture == nullptr)
                        break;

                    solid_quad_draw_info_t solid_quad{ pass.solid_quad };
                    if (pass.expand_to_target)
                    {
                        solid_quad.x = static_cast<float>(target.rect_px.position.x);
                        solid_quad.y = static_cast<float>(target.rect_px.position.y);
                        solid_quad.width = static_cast<float>(target.rect_px.size.x);
                        solid_quad.height = static_cast<float>(target.rect_px.size.y);
                    }

                    textured_quad_draw_info_t quad{
                        .texture = _solid_white_texture.get(),
                        .x = solid_quad.x,
                        .y = solid_quad.y,
                        .width = solid_quad.width,
                        .height = solid_quad.height,
                        .u0 = 0.f,
                        .v0 = 0.f,
                        .u1 = 1.f,
                        .v1 = 1.f,
                        .layer = render_layer_t::ui,
                        .order_mode = render_order_mode_t::explicit_order,
                        .order_in_layer = 0,
                        .sort_reference_y = 0.f,
                        .color = solid_quad.color,
                        .sampler_preset = solid_quad.sampler_preset
                    };

                    quad_instance_t instance{
                        build_quad_instance(non_world_stage_plan(non_world_stage_target_t::composite),
                                            quad_content_kind_t::textured,
                                            quad,
                                            { .domain = world_material_domain_t::unlit, .feature_flags = 0u },
                                            quad_target_for_composite_target(pass.target))
                    };
                    instance.submission_index = static_cast<std::uint64_t>(_bloom_source_quads.instances.size());
                    _bloom_source_quads.instances.push_back(instance);
                    break;
                }

                default:
                    CE_ASSERT(false, "Bloom source pass kind must be known");
                    break;
            }
        }
    }

    void renderer_t::compose_bloom_sources()
    {
        _stats.bloom_source_pass_count += static_cast<std::uint32_t>(_bloom_source_passes.size());
        if (_bloom_source_passes.empty())
            return;

        ensure_bloom_source_render_target();
        ensure_bloom_blur_render_target();
        if (!_bloom_source_render_target ||
            !_bloom_source_render_target->color_texture() ||
            !_bloom_blur_render_target ||
            !_bloom_blur_render_target->color_texture())
        return;

        materialize_bloom_source_passes();
        build_bloom_blur_passes();

        // The current bloom proof always composites back onto the gameplay
        // composite target. This is intentionally narrower than a final
        // generalized bloom authoring system.
        queue_post_effect_textured_pass(post_effect_domain_t::bloom,
                                        composite_target_kind_t::gameplay_present,
                                        textured_quad_draw_info_t{
                                            .texture = _bloom_source_render_target->color_texture(),
                                            .x = 0.0f,
                                            .y = 0.0f,
                                            .width = 1.0f,
                                            .height = 1.0f,
                                            .u0 = 0.f,
                                            .v0 = 0.f,
                                            .u1 = 1.f,
                                            .v1 = 1.f,
                                            .layer = render_layer_t::ui,
                                            .order_in_layer = 0,
                                            .effect_param0 = 10.0f,
                                            .color = 0xFFFFFFFFu,
                                            .sampler_preset = quad_sampler_preset_t::smooth_clamp
                                        },
                                        "bloom_composite",
                                        true);
    }

    void renderer_t::build_bloom_blur_passes()
    {
        _bloom_blur_horizontal_quads.instances.clear();
        _bloom_blur_vertical_quads.instances.clear();

        if (!_bloom_source_render_target ||
            !_bloom_source_render_target->color_texture() ||
            !_bloom_blur_render_target ||
            !_bloom_blur_render_target->color_texture())
        {
            return;
        }

        const composite_target_t target{ composite_target(composite_target_kind_t::gameplay_present) };
        const float target_width{ static_cast<float>(target.rect_px.size.x) };
        const float target_height{ static_cast<float>(target.rect_px.size.y) };
        if (target_width <= 0.f || target_height <= 0.f)
            return;

        textured_quad_draw_info_t horizontal_quad{
            .texture = _bloom_source_render_target->color_texture(),
            .x = static_cast<float>(target.rect_px.position.x),
            .y = static_cast<float>(target.rect_px.position.y),
            .width = target_width,
            .height = target_height,
            .u0 = 0.f,
            .v0 = 0.f,
            .u1 = 1.f,
            .v1 = 1.f,
            .layer = render_layer_t::ui,
            .order_mode = render_order_mode_t::explicit_order,
            .order_in_layer = 0,
            .sort_reference_y = 0.f,
            .color = 0xFFFFFFFFu,
            .effect_mode = 1.f,
            .effect_param0 = 2.5f / target_width,
            .sampler_preset = quad_sampler_preset_t::smooth_clamp
        };

        quad_instance_t horizontal_instance{
            build_quad_instance(non_world_stage_plan(non_world_stage_target_t::composite),
                                quad_content_kind_t::textured,
                                horizontal_quad,
                                { .domain = world_material_domain_t::unlit, .feature_flags = 0u },
                                quad_target_for_composite_target(composite_target_kind_t::gameplay_present))
        };
        horizontal_instance.submission_index = 0u;
        _bloom_blur_horizontal_quads.instances.push_back(horizontal_instance);

        textured_quad_draw_info_t vertical_quad{
            .texture = _bloom_blur_render_target->color_texture(),
            .x = static_cast<float>(target.rect_px.position.x),
            .y = static_cast<float>(target.rect_px.position.y),
            .width = target_width,
            .height = target_height,
            .u0 = 0.f,
            .v0 = 0.f,
            .u1 = 1.f,
            .v1 = 1.f,
            .layer = render_layer_t::ui,
            .order_mode = render_order_mode_t::explicit_order,
            .order_in_layer = 0,
            .sort_reference_y = 0.f,
            .color = 0xFFFFFFFFu,
            .effect_mode = -1.f,
            .effect_param0 = 2.5f / target_height,
            .sampler_preset = quad_sampler_preset_t::smooth_clamp
        };

        quad_instance_t vertical_instance{
            build_quad_instance(non_world_stage_plan(non_world_stage_target_t::composite),
                                quad_content_kind_t::textured,
                                vertical_quad,
                                { .domain = world_material_domain_t::unlit, .feature_flags = 0u },
                                quad_target_for_composite_target(composite_target_kind_t::gameplay_present))
        };
        vertical_instance.submission_index = 0u;
        _bloom_blur_vertical_quads.instances.push_back(vertical_instance);
    }

    void renderer_t::queue_post_effect_textured_pass(const post_effect_domain_t domain,
                                                     const composite_target_kind_t target_kind,
                                                     textured_quad_draw_info_t quad,
                                                     const char* const debug_name,
                                                     const bool expand_to_target)
    {
        quad.layer = render_layer_t::ui;
        quad.order_mode = render_order_mode_t::explicit_order;
        quad.order_in_layer = 0;
        quad.sort_reference_y = 0.f;

        _post_effect_passes.push_back(post_effect_pass_t{
            .kind = post_effect_pass_kind_t::textured,
            .domain = domain,
            .target = target_kind,
            .expand_to_target = expand_to_target,
            .textured_quad = quad,
            .solid_quad = { },
            .debug_name = debug_name
        });
    }

    void renderer_t::queue_post_effect_solid_pass(const post_effect_domain_t domain,
                                                  const composite_target_kind_t target_kind,
                                                  solid_quad_draw_info_t quad,
                                                  const char* const debug_name,
                                                  const bool expand_to_target)
    {
        quad.layer = render_layer_t::ui;
        quad.order_mode = render_order_mode_t::explicit_order;
        quad.order_in_layer = 0;
        quad.sort_reference_y = 0.f;

        _post_effect_passes.push_back(post_effect_pass_t{
            .kind = post_effect_pass_kind_t::solid,
            .domain = domain,
            .target = target_kind,
            .expand_to_target = expand_to_target,
            .textured_quad = { },
            .solid_quad = quad,
            .debug_name = debug_name
        });
    }

    void renderer_t::materialize_post_effect_passes()
    {
        _bloom_composite_quads.instances.clear();

        for (const post_effect_pass_t& pass : _post_effect_passes)
        {
            const composite_target_t target{ composite_target(pass.target) };
            switch (pass.kind)
            {
                case post_effect_pass_kind_t::textured:
                {
                    textured_quad_draw_info_t quad{ pass.textured_quad };
                    if (pass.expand_to_target)
                    {
                        quad.x = static_cast<float>(target.rect_px.position.x);
                        quad.y = static_cast<float>(target.rect_px.position.y);
                        quad.width = static_cast<float>(target.rect_px.size.x);
                        quad.height = static_cast<float>(target.rect_px.size.y);
                    }

                    if (pass.domain == post_effect_domain_t::bloom)
                    {
                        quad_instance_t instance{
                            build_quad_instance(non_world_stage_plan(non_world_stage_target_t::composite),
                                                quad_content_kind_t::textured,
                                                quad,
                                                { .domain = world_material_domain_t::unlit, .feature_flags = 0u },
                                                quad_target_for_composite_target(pass.target))
                        };
                        instance.submission_index = static_cast<std::uint64_t>(_bloom_composite_quads.instances.size());
                        _bloom_composite_quads.instances.push_back(instance);
                    }
                    else
                    {
                        submit_non_world_textured_quad(non_world_stage_target_t::composite,
                                                       quad,
                                                       quad_target_for_composite_target(pass.target));
                    }
                    break;
                }
                case post_effect_pass_kind_t::solid:
                {
                    solid_quad_draw_info_t quad{ pass.solid_quad };
                    if (pass.expand_to_target)
                    {
                        quad.x = static_cast<float>(target.rect_px.position.x);
                        quad.y = static_cast<float>(target.rect_px.position.y);
                        quad.width = static_cast<float>(target.rect_px.size.x);
                        quad.height = static_cast<float>(target.rect_px.size.y);
                    }

                    submit_non_world_solid_quad(non_world_stage_target_t::composite,
                                                quad,
                                                quad_target_for_composite_target(pass.target));
                    break;
                }
                default:
                    CE_ASSERT(false, "Composite fullscreen pass kind must be known");
                    break;
            }

            if (pass.domain == post_effect_domain_t::bloom)
                _stats.bloom_pass_count++;
        }

        _stats.post_effect_pass_count += static_cast<uint32_t>(_post_effect_passes.size());
    }

    void renderer_t::build_textured_quad_batches(textured_quad_state_t& state) const
    {
        state.instance_data_cpu.clear();
        state.batches.clear();

        if (state.instances.empty())
            return;

        std::stable_sort(state.instances.begin(), state.instances.end(), quad_instance_sorts_before);

        quad_bucket_key_t current_bucket_key{ };
        bool has_current_bucket{ false };
        for (const quad_instance_t& submission: state.instances)
        {
            const quad_bucket_key_t bucket_key{ quad_bucket_key(submission) };
            const bool starts_new_bucket{ !has_current_bucket || !(current_bucket_key == bucket_key) };
            if (starts_new_bucket)
            {
                state.batches.push_back(textured_quad_batch_t{
                    .texture = submission.texture,
                    .first_index = 0u,
                    .index_count = 6u,
                    .first_instance = static_cast<uint32_t>(state.instance_data_cpu.size()),
                    .instance_count = 0u,
                    .sampler_preset = submission.sampler_preset,
                    .world_material = submission.world_material
                });
                current_bucket_key = bucket_key;
                has_current_bucket = true;
            }

            state.instance_data_cpu.push_back(gpu_quad_instance_t{
                .quad_rect_px = {
                    submission.x,
                    submission.y,
                    submission.width,
                    submission.height
                },
                .uv_rect = {
                    submission.uv_rect.u_min,
                    submission.uv_rect.v_min,
                    submission.uv_rect.u_max,
                    submission.uv_rect.v_max
                },
                .color = unpack_abgr_color_to_float4(submission.color),
                .draw_params = {
                    submission.effect_mode,
                    submission.effect_param0,
                    0.f,
                    0.f
                }
            });

            state.batches.back().instance_count += 1u;
        }
    }

    void renderer_t::reset_stage_submission_group(const stage_submission_group_t& group) noexcept
    {
        const auto reset_state = [](textured_quad_state_t* state) noexcept
        {
            if (!state)
                return;

            state->instances.clear();
            state->instance_data_cpu.clear();
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

        ensure_shared_quad_geometry_buffers();
        const size_t instance_bytes{ stage_state.instance_data_cpu.size() * sizeof(gpu_quad_instance_t) };
        const auto transient_upload{
            _rhi->allocate_transient_vertex_upload(instance_bytes,
                                                   rhi::transient_upload::vertex_alignment<gpu_quad_instance_t>())
        };
        if (!_shared_quad_vertex_buffer || !_shared_quad_index_buffer || !transient_upload || !transient_upload->buffer ||
            !transient_upload->mapped_ptr)
            return;
        std::memcpy(transient_upload->mapped_ptr, stage_state.instance_data_cpu.data(), instance_bytes);

        rhi::textured_quad_stage_record_t final_record{ record };
        static_cast<rhi::quad_draw_source_t&>(final_record) = build_quad_draw_source_record(_shared_quad_vertex_buffer.get(),
                                                                                            _shared_quad_index_buffer.get(),
                                                                                            transient_upload->buffer,
                                                                                            nullptr,
                                                                                            static_cast<std::uint32_t>(stage_state.instance_data_cpu.size()),
                                                                                            static_cast<std::uint32_t>(transient_upload->offset_bytes));
        final_record.batches = stage_state.batches;

        if (is_text)
            _rhi->record_text_quad_stage(final_record);
        else
            _rhi->record_textured_quad_stage(final_record);

        _stats.vertex_count += static_cast<uint32_t>(stage_state.instance_data_cpu.size() * 4u);
        _stats.index_count += static_cast<uint32_t>(stage_state.instance_data_cpu.size() * 6u);
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
        CE_ASSERT(group.textured != nullptr && group.text != nullptr,
                  "Renderer world stage must have textured and text submission state");
        const forward_plus_gpu_buffers_t& gpu_buffers{ current_forward_plus_gpu_buffers() };
        std::stable_sort(_world_quad_instances.instances.begin(),
                         _world_quad_instances.instances.end(),
                         quad_instance_sorts_before);

        _stats.world_render_item_count = static_cast<std::uint32_t>(_world_quad_instances.instances.size());
        const chlm::float2 visible_min{
            _active_camera.position.x,
            _active_camera.position.y
        };
        const chlm::float2 visible_max{
            _active_camera.position.x + resolved_world_camera.visible_world_size.x,
            _active_camera.position.y + resolved_world_camera.visible_world_size.y
        };

        group.textured->instances.clear();
        group.textured->instances.reserve(_world_quad_instances.instances.size());
        for (const quad_instance_t& instance : _world_quad_instances.instances)
        {
            if (!aabb_overlaps_aabb(instance.bounds_min_px, instance.bounds_max_px, visible_min, visible_max))
                continue;

            group.textured->instances.push_back(instance);
        }

        rhi::textured_quad_stage_record_t world_textured_record{ };
        static_cast<rhi::quad_stage_common_t&>(world_textured_record) = rhi::quad_stage_common_t{
            .view_projection = stage_context.view_projection,
            .ambient_color = _world_ambient_color,
            .forward_plus_constants = _world_forward_plus_constants,
            .forward_plus_light_input = _world_forward_plus_light_input,
            .forward_plus_output = _world_forward_plus_output,
            .forward_plus_light_input_buffer = gpu_buffers.light_input_buffer.get(),
            .forward_plus_output_buffer = gpu_buffers.classification_output_buffer.get(),
            .world_item_buffer = nullptr,
            .visible_item_index_buffer = nullptr,
            .world_draw_mode = 0u,
            .viewport = stage_context.viewport,
            .presentation_mask = world_stage_plan.presentation_mask
        };
        record_stage_state(*group.textured, world_textured_record, false);
        _stats.world_textured_batch_count = static_cast<std::uint32_t>(group.textured->batches.size());

        rhi::textured_quad_stage_record_t world_text_record{ };
        static_cast<rhi::quad_stage_common_t&>(world_text_record) = build_quad_stage_common_record(stage_context,
                                                                                                    world_stage_plan.presentation_mask,
                                                                                                    &gpu_buffers);

        record_stage_state(*group.text, world_text_record, true);
    }

    void renderer_t::execute_frame_stage(const frame_stage_plan_t& stage_plan)
    {
        const stage_execution_context_t stage_context{ resolve_stage_execution_context(stage_plan) };

        if (stage_plan.kind == frame_stage_kind_t::composite)
        {
            if (!_bloom_source_quads.instances.empty() && _bloom_source_render_target)
            {
                rhi::textured_quad_stage_record_t bloom_source_record{ };
                static_cast<rhi::quad_stage_common_t&>(bloom_source_record) = build_quad_stage_common_record(stage_context, 0u);
                bloom_source_record.render_target = _bloom_source_render_target.get();
                bloom_source_record.target_load_action = rhi::quad_stage_common_t::target_load_action_t::clear;
                bloom_source_record.target_clear_color = { 0.f, 0.f, 0.f, 0.f };
                record_stage_state(_bloom_source_quads, bloom_source_record, false);
            }

            if (!_bloom_blur_horizontal_quads.instances.empty() && _bloom_blur_render_target)
            {
                rhi::textured_quad_stage_record_t bloom_horizontal_blur_record{ };
                static_cast<rhi::quad_stage_common_t&>(bloom_horizontal_blur_record) = build_quad_stage_common_record(stage_context, 0u);
                bloom_horizontal_blur_record.render_target = _bloom_blur_render_target.get();
                bloom_horizontal_blur_record.target_load_action = rhi::quad_stage_common_t::target_load_action_t::clear;
                bloom_horizontal_blur_record.target_clear_color = { 0.f, 0.f, 0.f, 0.f };
                bloom_horizontal_blur_record.shader_variant = rhi::quad_shader_variant_t::bloom_blur;
                record_stage_state(_bloom_blur_horizontal_quads, bloom_horizontal_blur_record, false);
            }

            if (!_bloom_blur_vertical_quads.instances.empty() && _bloom_source_render_target)
            {
                rhi::textured_quad_stage_record_t bloom_vertical_blur_record{ };
                static_cast<rhi::quad_stage_common_t&>(bloom_vertical_blur_record) = build_quad_stage_common_record(stage_context, 0u);
                bloom_vertical_blur_record.render_target = _bloom_source_render_target.get();
                bloom_vertical_blur_record.target_load_action = rhi::quad_stage_common_t::target_load_action_t::clear;
                bloom_vertical_blur_record.target_clear_color = { 0.f, 0.f, 0.f, 0.f };
                bloom_vertical_blur_record.shader_variant = rhi::quad_shader_variant_t::bloom_blur;
                record_stage_state(_bloom_blur_vertical_quads, bloom_vertical_blur_record, false);
            }

            materialize_post_effect_passes();
        }

        if (stage_plan.kind == frame_stage_kind_t::composite)
        {
            materialize_transition_passes(stage_context,
                                          stage_plan.presentation_mask,
                                          transition_pass_phase_t::pre_composite_record);
        }

        const stage_submission_group_t group{ stage_submission_group(stage_plan.kind) };
        CE_ASSERT(group.textured != nullptr && group.text != nullptr,
                  "Renderer non-world stage must have both textured and text submission state");

        const forward_plus_gpu_buffers_t& gpu_buffers{ current_forward_plus_gpu_buffers() };
        rhi::textured_quad_stage_record_t record{ };
        static_cast<rhi::quad_stage_common_t&>(record) = build_quad_stage_common_record(stage_context,
                                                                                        stage_plan.presentation_mask,
                                                                                        &gpu_buffers);

        record_stage_state(*group.textured, record, false);
        record_stage_state(*group.text, record, true);

        if (stage_plan.kind == frame_stage_kind_t::composite && !_bloom_composite_quads.instances.empty())
        {
            rhi::textured_quad_stage_record_t bloom_composite_record{ };
            static_cast<rhi::quad_stage_common_t&>(bloom_composite_record) = build_quad_stage_common_record(
                stage_context,
                stage_plan.presentation_mask,
                &gpu_buffers);
            bloom_composite_record.shader_variant = rhi::quad_shader_variant_t::bloom_composite;
            record_stage_state(_bloom_composite_quads, bloom_composite_record, false);
        }

        if (stage_plan.kind == frame_stage_kind_t::composite)
        {
            materialize_transition_passes(stage_context,
                                          stage_plan.presentation_mask,
                                          transition_pass_phase_t::post_composite_record);
        }
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
            stage_state.instances.clear();
            stage_state.instance_data_cpu.clear();
            stage_state.batches.clear();
        };

        for (const frame_stage_plan_t& stage_plan : _frame_stage_plan)
        {
            const stage_submission_group_t group{ stage_submission_group(stage_plan.kind) };
            if (group.textured)
                release_stage_buffers(*group.textured);
            if (group.text)
                release_stage_buffers(*group.text);
        }

        release_stage_buffers(_world_textured_quads);
        release_stage_buffers(_world_text_quads);
        release_stage_buffers(_bloom_source_quads);
        release_stage_buffers(_bloom_blur_horizontal_quads);
        release_stage_buffers(_bloom_blur_vertical_quads);
        release_stage_buffers(_bloom_composite_quads);

        for (forward_plus_gpu_buffers_t& frame_buffers : _forward_plus_gpu_buffers)
        {
            frame_buffers.constants_buffer.reset();
            frame_buffers.light_input_buffer.reset();
            frame_buffers.classification_output_buffer.reset();
        }

        _world_quad_instances.instances.clear();
        _bloom_source_quads.instances.clear();
        _bloom_blur_horizontal_quads.instances.clear();
        _bloom_blur_vertical_quads.instances.clear();
        _bloom_composite_quads.instances.clear();
        _bloom_source_passes.clear();
        _post_effect_passes.clear();
        _transition_passes.clear();
        _shared_quad_vertex_buffer.reset();
        _shared_quad_index_buffer.reset();
        _bloom_source_render_target.reset();
        _bloom_blur_render_target.reset();
        _transition_battle_swirl_capture_texture.reset();

        _stats = { };
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

    void renderer_t::ensure_shared_quad_geometry_buffers()
    {
        if (!_rhi || (_shared_quad_vertex_buffer && _shared_quad_index_buffer))
            return;

        constexpr std::array<quad_vertex_t, 4> quad_vertices{
            quad_vertex_t{ .x = 0.f, .y = 0.f, .u = 0.f, .v = 0.f, .color = 0xFFFFFFFFu, .effect_mode = 0.f, .effect_param0 = 0.f },
            quad_vertex_t{ .x = 1.f, .y = 0.f, .u = 1.f, .v = 0.f, .color = 0xFFFFFFFFu, .effect_mode = 0.f, .effect_param0 = 0.f },
            quad_vertex_t{ .x = 1.f, .y = 1.f, .u = 1.f, .v = 1.f, .color = 0xFFFFFFFFu, .effect_mode = 0.f, .effect_param0 = 0.f },
            quad_vertex_t{ .x = 0.f, .y = 1.f, .u = 0.f, .v = 1.f, .color = 0xFFFFFFFFu, .effect_mode = 0.f, .effect_param0 = 0.f }
        };
        constexpr std::array<std::uint32_t, 6> quad_indices{ 0u, 1u, 2u, 0u, 2u, 3u };

        _shared_quad_vertex_buffer = _rhi->create_buffer({
            .size_bytes = quad_vertices.size() * sizeof(quad_vertex_t),
            .usage = rhi::buffer_usage_t::vertex,
            .initial_data = quad_vertices.data()
        });
        _shared_quad_index_buffer = _rhi->create_buffer({
            .size_bytes = quad_indices.size() * sizeof(std::uint32_t),
            .usage = rhi::buffer_usage_t::index,
            .initial_data = quad_indices.data()
        });

        if (!_shared_quad_vertex_buffer || !_shared_quad_index_buffer)
            LOG_GRAPHICS_FATAL("Failed to create renderer shared quad geometry buffers");
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

    void renderer_t::ensure_bloom_source_render_target()
    {
        if (!_rhi)
            return;

        const chlm::uint2 target_size{ current_render_target_size() };
        if (target_size.x == 0u || target_size.y == 0u)
            return;

        if (_bloom_source_render_target &&
            _bloom_source_render_target->width() == target_size.x &&
            _bloom_source_render_target->height() == target_size.y)
        {
            return;
        }

        _bloom_source_render_target = _rhi->create_render_target_2d({
            .width = target_size.x,
            .height = target_size.y,
            .format = rhi::texture_format_t::rgba8_srgb,
            .shader_readable = true
        });

        if (!_bloom_source_render_target)
            LOG_GRAPHICS_FATAL("Failed to create bloom source render target");
    }

    void renderer_t::ensure_bloom_blur_render_target()
    {
        if (!_rhi)
            return;

        const chlm::uint2 target_size{ current_render_target_size() };
        if (target_size.x == 0u || target_size.y == 0u)
            return;

        if (_bloom_blur_render_target &&
            _bloom_blur_render_target->width() == target_size.x &&
            _bloom_blur_render_target->height() == target_size.y)
        {
            return;
        }

        _bloom_blur_render_target = _rhi->create_render_target_2d({
            .width = target_size.x,
            .height = target_size.y,
            .format = rhi::texture_format_t::rgba8_srgb,
            .shader_readable = true
        });

        if (!_bloom_blur_render_target)
            LOG_GRAPHICS_FATAL("Failed to create bloom blur render target");
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

    forward_plus_gpu_buffers_t& renderer_t::current_forward_plus_gpu_buffers() noexcept
    {
        return _forward_plus_gpu_buffers[current_textured_quad_frame_buffer_slot()];
    }

    const forward_plus_gpu_buffers_t& renderer_t::current_forward_plus_gpu_buffers() const noexcept
    {
        return _forward_plus_gpu_buffers[current_textured_quad_frame_buffer_slot()];
    }

} // namespace carrot::renderer
