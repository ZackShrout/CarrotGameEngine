//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/Shaders/VFSShaderFileProvider.h"
#include "Camera/Camera2D.h"
#include "Core/Module.h"
#include "Draw/QuadTypes.h"
#include "Draw/QuadInstanceData.h"
#include "Draw/TexturedQuadBatch.h"
#include "Draw/TexturedQuadCameraUniform.h"
#include "Draw/TexturedQuadEffectShared.h"
#include "Draw/TexturedQuadTypes.h"
#include "EngineConfig.h"
#include "Primitives/QuadVertex.h"
#include "RHI/RHI.h"
#include "Window/Window.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::rhi {
    class rhi_buffer_t;
}

namespace carrot::assets {
    class loaded_tilemap_asset_t;
}

namespace carrot::world {
    class world_t;
    class world_object_t;
    struct layering_debug_snapshot_t;
}

namespace carrot::renderer {
    constexpr std::size_t k_textured_quad_frame_buffer_count{ 3 };

    struct sprite_draw_info_t;

    struct tilemap_draw_info_t
    {
        const assets::loaded_tilemap_asset_t* tilemap{ nullptr };
        chlm::float2 origin{ 0.f, 0.f };
        chlm::float2 scale{ 1.f, 1.f };
        float source_pixels_per_unit{ 0.f };
        float render_pixels_per_unit{ 0.f };
        bool include_object_layers{ true };
        std::span<const std::string_view> active_visibility_tags{ };
        render_layer_t layer{ render_layer_t::world_back };
        render_order_mode_t order_mode{ render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        float sort_reference_y{ 0.f };
        quad_sampler_preset_t sampler_preset{ quad_sampler_preset_t::pixel_clamp };
        uint32_t color{ 0xFFFFFFFFu };
    };

    struct solid_quad_draw_info_t
    {
        float x{ 0.f };
        float y{ 0.f };
        float width{ 1.f };
        float height{ 1.f };
        render_layer_t layer{ render_layer_t::debug };
        render_order_mode_t order_mode{ render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        float sort_reference_y{ 0.f };
        uint32_t color{ 0xFFFFFFFFu };
        quad_sampler_preset_t sampler_preset{ quad_sampler_preset_t::pixel_clamp };
    };

    struct renderer_stats_t
    {
        uint32_t draw_calls{ 0 };
        uint32_t textured_quad_count{ 0 };
        uint32_t textured_quad_batch_count{ 0 };
        uint32_t bloom_source_pass_count{ 0 };
        uint32_t post_effect_pass_count{ 0 };
        uint32_t bloom_pass_count{ 0 };
        uint32_t world_render_item_count{ 0 };
        uint32_t world_textured_batch_count{ 0 };
        uint32_t active_visibility_tag_count{ 0 };
        uint32_t visible_layer_count{ 0 };
        uint32_t hidden_layer_count{ 0 };
        uint32_t vertex_count{ 0 };
        uint32_t index_count{ 0 };
        uint32_t world_point_light_count{ 0 };
        uint32_t dropped_world_point_light_count{ 0 };
        uint32_t forward_plus_tile_count{ 0 };
        uint32_t forward_plus_light_index_count{ 0 };
        uint32_t forward_plus_dropped_light_references{ 0 };
    };

    struct bloom_settings_t
    {
        bool enabled{ false };
        float baseline_strength{ 0.0f };
        float peak_light_response{ 0.12f };
        float accumulated_light_response{ 0.08f };
        float ambient_response{ 0.06f };
        float max_strength{ 0.28f };
        uint32_t tint_abgr{ 0xFFFFFFFFu };
    };

    struct textured_quad_state_t
    {
        struct frame_buffers_t
        {
            std::unique_ptr<rhi::rhi_buffer_t> instance_buffer;
            size_t instance_capacity{ 0 };
        };

        std::vector<quad_instance_t> instances;

        // CPU-side submission/batching for the current frame
        std::vector<gpu_quad_instance_t> instance_data_cpu;
        std::vector<textured_quad_batch_t> batches;

        // Per-frame reusable geometry buffers
        std::array<frame_buffers_t, k_textured_quad_frame_buffer_count> frame_buffers;
    };

    struct forward_plus_gpu_buffers_t
    {
        std::unique_ptr<rhi::rhi_buffer_t> constants_buffer;
        std::unique_ptr<rhi::rhi_buffer_t> light_input_buffer;
        std::unique_ptr<rhi::rhi_buffer_t> classification_output_buffer;
    };

    enum class composite_target_kind_t : std::uint8_t
    {
        gameplay_present = 0
    };

    struct composite_target_t
    {
        composite_target_kind_t kind{ composite_target_kind_t::gameplay_present };
        chlm::uint2 pixel_size{ 1u, 1u };
        chlm::uint_rect rect_px{
            .position = { 0u, 0u },
            .size = { 1u, 1u }
        };
        const char* debug_name{ "gameplay_present" };
    };

    enum class post_effect_pass_kind_t : std::uint8_t
    {
        textured = 0,
        solid
    };

    enum class post_effect_domain_t : std::uint8_t
    {
        overlay = 0,
        bloom
    };

    // Milestone 27 bloom scope:
    // these are authored in composite/render-target pixel space and feed the
    // offscreen bloom proof pipeline. They are not yet a generalized world-space
    // emissive/bloom contract.
    struct bloom_source_pass_t
    {
        post_effect_pass_kind_t kind{ post_effect_pass_kind_t::solid };
        composite_target_kind_t target{ composite_target_kind_t::gameplay_present };
        bool expand_to_target{ false };
        textured_quad_draw_info_t textured_quad{ };
        solid_quad_draw_info_t solid_quad{ };
        const char* debug_name{ "bloom_source_pass" };
    };

    struct post_effect_pass_t
    {
        post_effect_pass_kind_t kind{ post_effect_pass_kind_t::solid };
        post_effect_domain_t domain{ post_effect_domain_t::overlay };
        composite_target_kind_t target{ composite_target_kind_t::gameplay_present };
        bool expand_to_target{ false };
        textured_quad_draw_info_t textured_quad{ };
        solid_quad_draw_info_t solid_quad{ };
        const char* debug_name{ "post_effect_pass" };
    };

    enum class transition_pass_kind_t : std::uint8_t
    {
        fade = 0,
        wipe,
        battle_swirl
    };

    enum class transition_pass_phase_t : std::uint8_t
    {
        pre_composite_record = 0,
        post_composite_record
    };

    struct transition_pass_t
    {
        transition_pass_kind_t kind{ transition_pass_kind_t::fade };
        composite_target_kind_t target{ composite_target_kind_t::gameplay_present };
        uint32_t color_abgr{ 0x00000000u };
        float progress{ 0.f };
        float coverage{ 0.f };
        std::uint8_t wipe_direction{ 0u };
        bool incoming{ false };
        const char* debug_name{ "transition_pass" };
    };

    struct battle_swirl_state_t
    {
        bool enabled{ false };
        float progress{ 0.f };
        bool incoming{ false };
    };

    enum class transition_wipe_direction_t : std::uint8_t
    {
        left_to_right = 0,
        right_to_left,
        top_to_bottom,
        bottom_to_top
    };

    struct transition_wipe_state_t
    {
        bool enabled{ false };
        float coverage{ 0.f };
        transition_wipe_direction_t direction{ transition_wipe_direction_t::left_to_right };
        uint32_t color_abgr{ 0x00000000u };
    };

    struct light_shaft_readiness_t
    {
        bool renderer_contract_ready{ true };
        bool composite_capture_source_available{ true };
        bool fullscreen_pass_orchestration_available{ true };
        bool point_light_source_input_available{ true };
        bool requires_world_occlusion_mask{ true };
        bool requires_source_mask_texture{ true };
        bool requires_authored_shaft_source_selection{ true };
    };

    class renderer_t final : public core::module_t
    {
    public:
        CARROT_MODULE_NAME("Renderer")

        explicit renderer_t(io::virtual_file_system_t& vfs,
                            const engine_graphics_config_t& config,
                            window::window_id_t render_window_id = window::invalid_window_id);
        ~renderer_t() override { shutdown(); }

        void init() override;
        void shutdown() override;

        // Main loop integration
        void begin_frame();
        void end_frame();
        bool add_presentation_window(window::window_id_t window_id,
                                     uint32_t presentation_channel_mask = rhi::presentation_channel_gameplay);
        bool remove_presentation_window(window::window_id_t window_id);

        void set_camera_2d(const camera_2d_t& camera) noexcept { _active_camera = camera; }

        // Very high-level drawing commands (immediate mode for now)
        void draw_textured_quad(const textured_quad_draw_info_t& quad);
        void draw_overlay_textured_quad(const textured_quad_draw_info_t& quad);
        void draw_ui_textured_quad(const textured_quad_draw_info_t& quad);
        void draw_composite_textured_quad(const textured_quad_draw_info_t& quad);
        void draw_log_console_textured_quad(const textured_quad_draw_info_t& quad);
        void draw_bloom_textured_quad(const textured_quad_draw_info_t& quad);
        void draw_text_quad(const textured_quad_draw_info_t& quad);
        void draw_overlay_text_quad(const textured_quad_draw_info_t& quad);
        void draw_ui_text_quad(const textured_quad_draw_info_t& quad);
        void draw_log_console_text_quad(const textured_quad_draw_info_t& quad);
        void draw_solid_quad(const solid_quad_draw_info_t& quad);
        void draw_overlay_solid_quad(const solid_quad_draw_info_t& quad);
        void draw_ui_solid_quad(const solid_quad_draw_info_t& quad);
        void draw_composite_solid_quad(const solid_quad_draw_info_t& quad);
        void draw_log_console_solid_quad(const solid_quad_draw_info_t& quad);
        void draw_bloom_solid_quad(const solid_quad_draw_info_t& quad);
        void set_bloom_settings(const bloom_settings_t& settings) noexcept;
        [[nodiscard]] const bloom_settings_t& bloom_settings() const noexcept { return _bloom_settings; }
        void set_transition_fade_color(uint32_t color_abgr) noexcept;
        void clear_transition_fade() noexcept;
        void set_transition_wipe(float coverage, transition_wipe_direction_t direction, uint32_t color_abgr) noexcept;
        void clear_transition_wipe() noexcept;
        void set_transition_battle_swirl(float progress, bool incoming) noexcept;
        void clear_transition_battle_swirl() noexcept;
        void set_composite_overlay_color(uint32_t color_abgr) noexcept;
        void clear_composite_overlay() noexcept;
        void draw_sprite(const sprite_draw_info_t& info);
        void draw_tilemap(const tilemap_draw_info_t& info);
        void draw_world(const world::world_t& world);

        // Hot-reload & debug support
        void notify_shader_changed(std::string_view path);

        // Basic stats & introspection
        [[nodiscard]] const renderer_stats_t& get_stats() const noexcept { return _stats; }
        [[nodiscard]] const renderer_stats_t& get_last_completed_stats() const noexcept { return _last_completed_stats; }
        [[nodiscard]] uint64_t get_frame_index() const noexcept { return _frame_index; }
        [[nodiscard]] rhi::rhi_context_t* get_rhi() const noexcept { return _rhi.get(); }
        [[nodiscard]] light_shaft_readiness_t light_shaft_readiness() const noexcept;
        [[nodiscard]] std::size_t pending_world_render_item_count() const noexcept { return _world_quad_instances.instances.size(); }
        [[nodiscard]] std::size_t pending_post_effect_pass_count() const noexcept
        {
            return _post_effect_passes.size();
        }
        [[nodiscard]] std::size_t pending_bloom_source_pass_count() const noexcept
        {
            return _bloom_source_passes.size();
        }
        [[nodiscard]] std::size_t pending_transition_pass_count() const noexcept
        {
            return _transition_passes.size();
        }
        [[nodiscard]] rhi::graphics_api get_graphics_api() const noexcept
        {
            return _rhi ? _rhi->get_graphics_api() : _config.api;
        }
        [[nodiscard]] const camera_2d_t& get_camera_2d() const noexcept { return _active_camera; }
        [[nodiscard]] rhi::rhi_texture_t* debug_bloom_source_texture() const noexcept
        {
            return _bloom_source_render_target ? _bloom_source_render_target->color_texture() : nullptr;
        }
        [[nodiscard]] rhi::rhi_texture_t* debug_bloom_blur_texture() const noexcept
        {
            return _bloom_blur_render_target ? _bloom_blur_render_target->color_texture() : nullptr;
        }
        [[nodiscard]] chlm::uint2 current_render_target_pixel_size() const noexcept { return current_render_target_size(); }
        [[nodiscard]] resolved_camera_2d_t resolve_camera_2d() const noexcept
        {
            return _active_camera.resolve(current_render_target_size());
        }

    private:
        struct stage_execution_context_t
        {
            chlm::float4x4 view_projection{ chlm::float4x4::identity() };
            rhi::render_viewport_t viewport{ };
        };

        struct world_stage_draw_context_t;
        enum class non_world_stage_target_t : std::uint8_t
        {
            ui = 0,
            composite,
            overlay_debug,
            log_console
        };

        struct frame_stage_plan_t
        {
            frame_stage_kind_t kind{ frame_stage_kind_t::world };
            frame_stage_space_t space{ frame_stage_space_t::world_camera };
            std::uint32_t presentation_mask{ rhi::presentation_channel_gameplay };
            bool lighting_aware{ false };
            const char* debug_name{ "world" };
        };

        struct stage_submission_group_t
        {
            textured_quad_state_t* textured{ nullptr };
            textured_quad_state_t* text{ nullptr };
        };

        [[nodiscard]] chlm::uint2 current_render_target_size() const noexcept;
        [[nodiscard]] const frame_stage_plan_t& stage_plan(frame_stage_kind_t stage) const noexcept;
        [[nodiscard]] const frame_stage_plan_t& non_world_stage_plan(non_world_stage_target_t target) const noexcept;
        [[nodiscard]] stage_submission_group_t stage_submission_group(frame_stage_kind_t stage) noexcept;
        [[nodiscard]] stage_execution_context_t resolve_stage_execution_context(const frame_stage_plan_t& stage_plan) const noexcept;
        [[nodiscard]] composite_target_t composite_target(composite_target_kind_t kind) const noexcept;
        void validate_shared_renderer_limits() const noexcept;
        void validate_frame_stage_plan() const noexcept;
        void refresh_composite_targets() noexcept;
        [[nodiscard]] float resolve_bloom_strength() const noexcept;
        void queue_bloom_if_needed();
        void queue_bloom_source_textured_pass(composite_target_kind_t target,
                                              textured_quad_draw_info_t quad,
                                              const char* debug_name,
                                              bool expand_to_target = false);
        void queue_bloom_source_solid_pass(composite_target_kind_t target,
                                           solid_quad_draw_info_t quad,
                                           const char* debug_name,
                                           bool expand_to_target = false);
        void materialize_bloom_source_passes();
        void compose_bloom_sources();
        void build_bloom_blur_passes();
        void queue_transition_fade_if_needed();
        void queue_transition_wipe_if_needed();
        void queue_transition_battle_swirl_if_needed();
        void queue_composite_overlay_if_needed();
        void queue_post_effect_textured_pass(post_effect_domain_t domain,
                                             composite_target_kind_t target,
                                             textured_quad_draw_info_t quad,
                                             const char* debug_name,
                                             bool expand_to_target = false);
        void queue_post_effect_solid_pass(post_effect_domain_t domain,
                                          composite_target_kind_t target,
                                          solid_quad_draw_info_t quad,
                                          const char* debug_name,
                                          bool expand_to_target = false);
        void materialize_post_effect_passes();
        void materialize_transition_passes(const stage_execution_context_t& stage_context,
                                          uint32_t presentation_mask,
                                          transition_pass_phase_t phase);
        void record_transition_fade_pass(const transition_pass_t& pass);
        void record_transition_wipe_pass(const transition_pass_t& pass);
        void record_transition_battle_swirl_pass(const transition_pass_t& pass,
                                                 const stage_execution_context_t& stage_context,
                                                 uint32_t presentation_mask);
        [[nodiscard]] quad_instance_t build_quad_instance(const frame_stage_plan_t& stage_plan,
                                                          quad_content_kind_t content_kind,
                                                          const textured_quad_draw_info_t& quad,
                                                          world_material_key_t world_material,
                                                          quad_target_id_t target_id = {}) const;
        [[nodiscard]] rhi::quad_stage_common_t build_quad_stage_common_record(
            const stage_execution_context_t& stage_context,
            std::uint32_t presentation_mask,
            const forward_plus_gpu_buffers_t* gpu_buffers = nullptr) const;
        [[nodiscard]] rhi::quad_draw_source_t build_quad_draw_source_record(const rhi::rhi_buffer_t* vertex_buffer,
                                                                            const rhi::rhi_buffer_t* index_buffer,
                                                                            const rhi::rhi_buffer_t* instance_buffer = nullptr,
                                                                            const rhi::rhi_buffer_t* indirect_buffer = nullptr,
                                                                            std::uint32_t instance_count = 0u,
                                                                            std::uint32_t indirect_buffer_offset_bytes = 0u) const;
        void extract_world_render_item(const textured_quad_draw_info_t& quad, world_material_key_t world_material);
        void submit_world_textured_quad(const textured_quad_draw_info_t& quad);
        void submit_world_text_quad(const textured_quad_draw_info_t& quad);
        void submit_stage_textured_quad(const frame_stage_plan_t& stage_plan,
                                        const textured_quad_draw_info_t& quad,
                                        quad_target_id_t target_id = {});
        void submit_stage_text_quad(const frame_stage_plan_t& stage_plan,
                                    const textured_quad_draw_info_t& quad,
                                    quad_target_id_t target_id = {});
        void submit_non_world_textured_quad(non_world_stage_target_t target,
                                            const textured_quad_draw_info_t& quad,
                                            quad_target_id_t target_id = {});
        void submit_non_world_text_quad(non_world_stage_target_t target,
                                        const textured_quad_draw_info_t& quad,
                                        quad_target_id_t target_id = {});
        void submit_non_world_solid_quad(non_world_stage_target_t target,
                                         const solid_quad_draw_info_t& quad,
                                         quad_target_id_t target_id = {});
        void submit_solid_quad(frame_stage_kind_t stage, const solid_quad_draw_info_t& quad, quad_target_id_t target_id = {});
        void build_textured_quad_batches(textured_quad_state_t& state) const;
        void reset_stage_submission_group(const stage_submission_group_t& group) noexcept;
        void record_stage_state(textured_quad_state_t& stage_state,
                                const rhi::textured_quad_stage_record_t& record,
                                bool is_text);
        void execute_world_frame_stage();
        void execute_frame_stage(const frame_stage_plan_t& stage_plan);
        void execute_frame_stages();
        void release_frame_resources();
        void ensure_textured_quad_frame_buffers(textured_quad_state_t& state);
        void upload_textured_quad_frame_data(const textured_quad_state_t& state) const;
        void ensure_forward_plus_gpu_buffers();
        void upload_forward_plus_gpu_data() const;
        void update_forward_plus_diagnostics() noexcept;
        void ensure_shared_quad_geometry_buffers();
        void ensure_bloom_source_render_target();
        void ensure_bloom_blur_render_target();
        void ensure_transition_battle_swirl_capture_texture();
        void prepare_world_stage_context(const world::world_t& world, world_stage_draw_context_t& context);
        void submit_world_object(const world::world_object_t& object, world_stage_draw_context_t& context);
        void finalize_world_stage_context(world_stage_draw_context_t& context) const;
        void submit_tilemap(const tilemap_draw_info_t& info, world::layering_debug_snapshot_t* layering_debug_snapshot = nullptr);
        void submit_tile_object(const assets::loaded_tilemap_asset_t& tilemap,
                                uint32_t gid,
                                const chlm::float2& position_px,
                                const chlm::float2& size_px,
                                render_layer_t layer,
                                render_order_mode_t order_mode,
                                int32_t order_in_layer,
                                float sort_reference_y,
                                quad_sampler_preset_t sampler_preset,
                                uint32_t color);
        [[nodiscard]] uint32_t current_textured_quad_frame_buffer_slot() const noexcept;
        [[nodiscard]] textured_quad_state_t::frame_buffers_t& current_frame_buffers(textured_quad_state_t& state) const noexcept;
        [[nodiscard]] const textured_quad_state_t::frame_buffers_t& current_frame_buffers(const textured_quad_state_t& state) const noexcept;
        [[nodiscard]] forward_plus_gpu_buffers_t& current_forward_plus_gpu_buffers() noexcept;
        [[nodiscard]] const forward_plus_gpu_buffers_t& current_forward_plus_gpu_buffers() const noexcept;
        // ── External context / configuration ──────────────────────────────────────
        io::virtual_file_system_t&  _vfs;
        engine_graphics_config_t    _config;
        window::window_id_t         _render_window_id{ window::invalid_window_id };

        // ── Backend integration ───────────────────────────────────────────────────
        std::unique_ptr<rhi::rhi_context_t>                 _rhi;
        std::unique_ptr<rhi::rhi_compute_pipeline_t>        _forward_plus_classify_pipeline;
        std::unique_ptr<assets::vfs_shader_file_provider_t> _shader_provider;

        // ── Frame progression / stats ─────────────────────────────────────────────
        uint64_t            _frame_index{ 0 };
        renderer_stats_t    _stats;
        renderer_stats_t    _last_completed_stats;
        uint64_t            _animated_tiles_elapsed_ms{ 0 };
        std::chrono::steady_clock::time_point _animated_tiles_clock_origin{ std::chrono::steady_clock::now() };

        // ── Renderer path state: textured quads ───────────────────────────────────
        textured_quad_state_t _world_textured_quads;
        textured_quad_state_t _world_text_quads;
        textured_quad_state_t _bloom_source_quads;
        textured_quad_state_t _bloom_blur_horizontal_quads;
        textured_quad_state_t _bloom_blur_vertical_quads;
        textured_quad_state_t _bloom_composite_quads;
        quad_stream_t _world_quad_instances;
        std::array<textured_quad_state_t, static_cast<size_t>(frame_stage_kind_t::count)> _stage_textured_quads;
        std::array<textured_quad_state_t, static_cast<size_t>(frame_stage_kind_t::count)> _stage_text_quads;
        std::array<forward_plus_gpu_buffers_t, k_textured_quad_frame_buffer_count> _forward_plus_gpu_buffers;
        std::array<frame_stage_plan_t, static_cast<size_t>(frame_stage_kind_t::count)> _frame_stage_plan{
            frame_stage_plan_t{
                .kind = frame_stage_kind_t::world,
                .space = frame_stage_space_t::world_camera,
                .presentation_mask = rhi::presentation_channel_gameplay,
                .lighting_aware = true,
                .debug_name = "world"
            },
            frame_stage_plan_t{
                .kind = frame_stage_kind_t::ui,
                .space = frame_stage_space_t::render_target_pixels,
                .presentation_mask = rhi::presentation_channel_gameplay,
                .lighting_aware = false,
                .debug_name = "ui"
            },
            frame_stage_plan_t{
                .kind = frame_stage_kind_t::composite,
                .space = frame_stage_space_t::render_target_pixels,
                .presentation_mask = rhi::presentation_channel_gameplay,
                .lighting_aware = false,
                .debug_name = "composite"
            },
            frame_stage_plan_t{
                .kind = frame_stage_kind_t::overlay_debug,
                .space = frame_stage_space_t::viewport_pixels,
                .presentation_mask = rhi::presentation_channel_gameplay,
                .lighting_aware = false,
                .debug_name = "overlay_debug"
            },
            frame_stage_plan_t{
                .kind = frame_stage_kind_t::log_console,
                .space = frame_stage_space_t::render_target_pixels,
                .presentation_mask = rhi::presentation_channel_log_console,
                .lighting_aware = false,
                .debug_name = "log_console"
            }
        };

        camera_2d_t _active_camera{ };
        chlm::float4 _world_ambient_color{ 1.f, 1.f, 1.f, 1.f };
        forward_plus_light_input_t _world_forward_plus_light_input{ };
        forward_plus_frame_constants_t _world_forward_plus_constants{ };
        forward_plus_classification_output_t _world_forward_plus_output{ };
        std::unique_ptr<rhi::rhi_buffer_t> _shared_quad_vertex_buffer;
        std::unique_ptr<rhi::rhi_buffer_t> _shared_quad_index_buffer;
        std::unique_ptr<rhi::rhi_buffer_t> _transition_battle_swirl_instance_buffer;
        std::unique_ptr<rhi::rhi_texture_t> _solid_white_texture;
        std::unique_ptr<rhi::rhi_render_target_t> _bloom_source_render_target;
        std::unique_ptr<rhi::rhi_render_target_t> _bloom_blur_render_target;
        std::unique_ptr<rhi::rhi_texture_t> _transition_battle_swirl_capture_texture;
        bloom_settings_t _bloom_settings{ };
        bool _transition_fade_enabled{ false };
        uint32_t _transition_fade_color{ 0x00000000u };
        transition_wipe_state_t _transition_wipe{ };
        battle_swirl_state_t _transition_battle_swirl{ };
        bool _composite_overlay_enabled{ false };
        uint32_t _composite_overlay_color{ 0x00000000u };
        std::array<composite_target_t, 1u> _composite_targets{
            composite_target_t{
                .kind = composite_target_kind_t::gameplay_present,
                .pixel_size = { 1u, 1u },
                .rect_px = {
                    .position = { 0u, 0u },
                    .size = { 1u, 1u }
                },
                .debug_name = "gameplay_present"
            }
        };
        std::vector<bloom_source_pass_t> _bloom_source_passes;
        std::vector<post_effect_pass_t> _post_effect_passes;
        std::vector<transition_pass_t> _transition_passes;
    };
} // namespace carrot::renderer
