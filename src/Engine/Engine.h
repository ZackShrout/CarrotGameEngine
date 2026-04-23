//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetManager.h"
#include "Assets/Tilemap/TilemapAssetLoader.h"
#include "Audio/AudioModule.h"
#include "Core/Application.h"
#include "Core/CoreDefines.h"
#include "IO/VirtualFileSystem.h"
#include "Input/ControllerManager.h"
#include "Renderer/Renderer.h"
#include "RuntimeWindowSpecs.h"
#include "Utils/MulticastDelegate.h"
#include "Window/Window.h"
#include "World/World.h"

#include <string>
#include <future>
#include <vector>
#include <optional>
#include <filesystem>

#include "UI/UIModule.h"

namespace carrot {
    namespace rhi {
        class rhi_context_t;
    }

    namespace core {
        struct engine_paths_t;
        class ce_application_t;
        struct game_context_t;
        struct boot_prewarm_plan_t;
    }

    DECLARE_MULTICAST_DELEGATE(on_tick_t, float/* dt*/);

    enum class exit_code : int
    {
        success = 0,
        error = 1,
    };

    struct engine_profiling_snapshot_t
    {
        float frame_total_ms{ 0.f };
        float platform_poll_ms{ 0.f };
        float runtime_iteration_ms{ 0.f };
        float update_total_ms{ 0.f };
        float audio_update_ms{ 0.f };
        float ui_update_ms{ 0.f };
        float input_update_ms{ 0.f };
        float world_update_ms{ 0.f };
        float game_tick_ms{ 0.f };
        float render_total_ms{ 0.f };
        float begin_frame_ms{ 0.f };
        float world_render_ms{ 0.f };
        float ui_render_ms{ 0.f };
        float debug_render_ms{ 0.f };
        float log_console_render_ms{ 0.f };
        float end_frame_ms{ 0.f };
    };

    struct engine_profile_sample_t
    {
        std::uint64_t frame_index{ 0u };
        std::uint32_t fps{ 0u };
        engine_profiling_snapshot_t timing{ };
        renderer::renderer_stats_t renderer_stats{ };
    };

    class engine_t
    {
    public:
        engine_t() = default;
        ~engine_t();

        DISABLE_COPY_AND_MOVE(engine_t)

        void init();
        void init(const core::engine_paths_t& paths);
        int run(core::ce_application_t* app);

        void request_quit() noexcept { _should_quit = true; }
        [[nodiscard]] bool should_quit() const noexcept { return _should_quit; }

        [[nodiscard]] float get_delta_time() const noexcept { return _delta_time; }
        [[nodiscard]] uint32_t get_fps() const noexcept { return _current_fps; }
        [[nodiscard]] const engine_profiling_snapshot_t& get_last_profiling_snapshot() const noexcept
        {
            return _last_profiling_snapshot;
        }
        [[nodiscard]] const engine_profiling_snapshot_t& get_smoothed_profiling_snapshot() const noexcept
        {
            return _smoothed_profiling_snapshot;
        }
        [[nodiscard]] bool profiling_mode_enabled() const noexcept { return _profiling_mode_enabled; }
        [[nodiscard]] std::size_t profiling_sample_count() const noexcept { return _profiling_capture_samples.size(); }

        [[nodiscard]] assets::asset_manager_t& asset_manager() noexcept { return *_asset_manager; }
        [[nodiscard]] const assets::asset_manager_t& asset_manager() const noexcept { return *_asset_manager; }

        [[nodiscard]] renderer::renderer_t& renderer() noexcept { return *_renderer; }
        [[nodiscard]] const renderer::renderer_t& renderer() const noexcept { return *_renderer; }

        [[nodiscard]] world::world_t& world() noexcept { return _world; }
        [[nodiscard]] const world::world_t& world() const noexcept { return _world; }

        [[nodiscard]] io::virtual_file_system_t& vfs() noexcept { return _vfs; }
        [[nodiscard]] const io::virtual_file_system_t& vfs() const noexcept { return _vfs; }

    private:
        enum class boot_stage_t : uint8_t
        {
            pending = 0,
            discovering_manifests,
            registering_audio,
            registering_fonts,
            registering_textures,
            registering_sprites,
            registering_tilemaps,
            registering_scenes,
            expanding_scene_prewarm,
            prewarming_audio,
            prewarming_fonts,
            prewarming_textures,
            prewarming_sprites,
            prewarming_tilemaps,
            complete,
        };

        struct boot_pipeline_t
        {
            boot_stage_t stage{ boot_stage_t::pending };
            size_t completed_steps{ 0u };
            size_t total_steps{ 0u };
            size_t next_manifest_index{ 0u };
            std::vector<std::string> audio_manifests;
            std::vector<std::string> font_manifests;
            std::vector<std::string> texture_manifests;
            std::vector<std::string> sprite_manifests;
            std::vector<std::string> tilemap_manifests;
            std::vector<std::string> scene_manifests;
            core::boot_prewarm_plan_t prewarm_plan;
            std::string active_tilemap_prewarm_id;
            std::unique_ptr<std::future<assets::tilemap_asset_prepare_result_t>> tilemap_prepare_future;
        };

        struct runtime_window_instance_t
        {
            engine_runtime_window_role_t role{ engine_runtime_window_role_t::gameplay_main };
            window::window_id_t id{ window::invalid_window_id };
            bool is_main_window{ false };
            bool registered_for_presentation{ false };
            uint32_t presentation_channel_mask{ rhi::presentation_channel_gameplay };
            bool receives_gameplay_input{ false };
        };

        void tick(engine_profiling_snapshot_t& profiling);

        void render_world();
        void render_debug();
        void render_ui();
        void render_log_console();
        void accumulate_profiling_snapshot(const engine_profiling_snapshot_t& frame_snapshot) noexcept;
        void toggle_profiling_mode() noexcept;
        void clear_profiling_capture() noexcept;
        bool export_profiling_capture() const noexcept;
        [[nodiscard]] std::vector<engine_runtime_window_spec_t> build_runtime_window_specs(uint32_t width, uint32_t height) const;
        bool create_runtime_window(const engine_runtime_window_spec_t& spec);
        void bind_window_events(window::window_id_t window_id, window::window_id_t main_window_id);

        [[nodiscard]] core::engine_paths_t make_default_engine_paths() noexcept;
        [[nodiscard]] static std::optional<std::filesystem::path> find_repo_root(std::filesystem::path start) noexcept;
        void configure_paths(const core::engine_paths_t& paths);

        void initialize_boot_pipeline() noexcept;
        [[nodiscard]] bool advance_boot_pipeline();
        void render_boot_overlay() noexcept;
        [[nodiscard]] std::string_view boot_stage_label() const noexcept;
        [[nodiscard]] float boot_progress() const noexcept;
        void start_application(core::ce_application_t& app,
                               core::game_context_t& game,
                               window::window_id_t main_window_id);

        bool discover_and_register_assets();
        bool register_audio_asset_manifest(std::string_view manifest_uri);
        bool register_font_asset_manifest(std::string_view manifest_uri);
        bool register_texture_asset_manifest(std::string_view manifest_uri);
        bool register_sprite_asset_manifest(std::string_view manifest_uri);
        bool register_tilemap_asset_manifest(std::string_view manifest_uri);
        bool register_scene_asset_manifest(std::string_view manifest_uri);
        void destroy_closed_auxiliary_windows();

        bool                                                _initialized{ false };
        bool                                                _running{ false };
        bool                                                _application_started{ false };
        bool                                                _should_quit{ false };
        float                                               _delta_time{ 0.f };
        std::chrono::time_point<std::chrono::steady_clock>  _last_time_point{ };
        uint32_t                                            _current_fps{ 0 };
        engine_profiling_snapshot_t                         _last_profiling_snapshot{ };
        engine_profiling_snapshot_t                         _smoothed_profiling_snapshot{ };
        bool                                                _profiling_mode_enabled{ false };
        std::vector<engine_profile_sample_t>                _profiling_capture_samples;
        std::optional<std::filesystem::path>                _save_root;

        std::unique_ptr<renderer::renderer_t>               _renderer{ nullptr };
        std::unique_ptr<audio::audio_module_t>              _audio_module{ nullptr };
        std::unique_ptr<ui::ui_module_t>                    _ui_module{ nullptr };

        io::virtual_file_system_t                           _vfs;
        std::unique_ptr<assets::asset_manager_t>            _asset_manager{ nullptr };
        input::controller_manager_t                         _controller_manager;
        window::window_id_t                                 _gameplay_window_id{ window::invalid_window_id };
        std::vector<runtime_window_instance_t>              _runtime_windows;

        on_tick_t                                           _on_tick;
        renderer::renderer_stats_t                          _last_logged_renderer_stats;
        boot_pipeline_t                                     _boot_pipeline;
        std::optional<chlm::uint2>                          _pending_main_window_resize;

        world::world_t                                      _world;
    };
} // namespace carrot
