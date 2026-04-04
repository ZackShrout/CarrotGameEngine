//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetManager.h"
#include "Audio/AudioModule.h"
#include "Core/CoreDefines.h"
#include "IO/VirtualFileSystem.h"
#include "Renderer/Renderer.h"
#include "Utils/MulticastDelegate.h"
#include "World/World.h"

namespace carrot {
    namespace rhi {
        class rhi_context_t;
    }

    namespace core {
        struct engine_paths_t;
        class ce_application_t;
    }

    DECLARE_MULTICAST_DELEGATE(on_tick_t, float/* dt*/);

    enum class exit_code : int
    {
        success = 0,
        error = 1,
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
        [[nodiscard]] assets::asset_manager_t& asset_manager() noexcept { return *_asset_manager; }
        [[nodiscard]] const assets::asset_manager_t& asset_manager() const noexcept { return *_asset_manager; }
        [[nodiscard]] renderer::renderer_t& renderer() noexcept { return *_renderer; }
        [[nodiscard]] const renderer::renderer_t& renderer() const noexcept { return *_renderer; }
        [[nodiscard]] world::world_t& world() noexcept { return _world; }
        [[nodiscard]] const world::world_t& world() const noexcept { return _world; }
        [[nodiscard]] io::virtual_file_system_t& vfs() noexcept { return _vfs; }
        [[nodiscard]] const io::virtual_file_system_t& vfs() const noexcept { return _vfs; }

    private:
        void tick();

        void render_world();
        void render_debug();
        void render_ui();

        [[nodiscard]] core::engine_paths_t make_default_engine_paths() noexcept;
        [[nodiscard]] static std::optional<std::filesystem::path> find_repo_root(std::filesystem::path start) noexcept;
        void configure_paths(const core::engine_paths_t& paths);

        bool discover_and_register_assets();
        bool register_audio_asset_manifest(std::string_view manifest_uri);
        bool register_texture_asset_manifest(std::string_view manifest_uri);
        bool register_sprite_asset_manifest(std::string_view manifest_uri);
        bool register_tilemap_asset_manifest(std::string_view manifest_uri);
        bool register_scene_asset_manifest(std::string_view manifest_uri);

        bool                                                _initialized{ false };
        bool                                                _running{ false };
        bool                                                _should_quit{ false };
        float                                               _delta_time{ 0.f };
        std::chrono::time_point<std::chrono::steady_clock>  _last_time_point{ };
        uint32_t                                            _current_fps{ 0 };

        std::unique_ptr<renderer::renderer_t>               _renderer{ nullptr };
        std::unique_ptr<audio::audio_module_t>              _audio_module{ nullptr };

        io::virtual_file_system_t                           _vfs;
        std::unique_ptr<assets::asset_manager_t>            _asset_manager{ nullptr };

        on_tick_t                                           _on_tick;
        renderer::renderer_stats_t                          _last_logged_renderer_stats;

        world::world_t                                      _world;
    };
} // namespace carrot
