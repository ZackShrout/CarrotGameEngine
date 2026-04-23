//
// Created by zshrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "World/Import/TilemapWorldBridge.h"
#include "World.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace carrot::assets {
    class asset_manager_t;
    class loaded_sprite_asset_t;
    class loaded_tilemap_asset_t;
    struct scene_asset_record_t;
}

namespace carrot::core {
    struct game_context_t;
}

namespace carrot::world {
    /**
     * @brief Incrementally stages a world for a target scene without mutating the live runtime world.
     *
     * scene_load_task_t owns only the isolated loading/staging path:
     *
     * * resolve the scene asset and its dependencies
     * * construct a staged world
     * * import authored map/object data into that staged world
     * * resolve the effective spawn marker inside the staged world
     *
     * It does not own:
     *
     * * the currently active runtime scene
     * * transition presentation
     * * controller binding
     * * camera or music application
     * * deciding when the staged world should replace the active world
     *
     * Those remain responsibilities of scene::scene_runtime_t.
     */
    class scene_load_task_t
    {
    public:
        scene_load_task_t(std::string_view scene_id,
                          std::string_view spawn_marker_override = {});

        [[nodiscard]] bool advance(assets::asset_manager_t& assets);
        [[nodiscard]] bool is_ready_to_activate() const noexcept;
        [[nodiscard]] bool is_complete() const noexcept;
        [[nodiscard]] bool has_failed() const noexcept;
        [[nodiscard]] bool is_background_preparing() const noexcept;
        [[nodiscard]] std::string_view scene_id() const noexcept { return _scene_id; }
        [[nodiscard]] std::string_view effective_spawn_marker() const noexcept { return _effective_spawn_marker; }
        [[nodiscard]] const assets::scene_asset_record_t* scene_record() const noexcept { return _scene_record; }
        [[nodiscard]] size_t completed_steps() const noexcept;
        [[nodiscard]] size_t total_steps() const noexcept;
        [[nodiscard]] world_t take_world();

    private:
        enum class phase_t : uint8_t
        {
            resolve_scene = 0,
            resolve_dependencies,
            initialize_world,
            start_background_prepare,
            await_background_prepare,
            activate_authored_content,
            resolve_spawn,
            ready_to_activate,
            complete,
            failed
        };

        struct background_prepare_state_t
        {
            std::mutex mutex;
            std::optional<import::prepared_tilemap_world_data_t> prepared_data;
            bool complete{ false };
        };

        void fail() noexcept;

        std::string _scene_id;
        std::string _spawn_marker_override;
        std::string _effective_spawn_marker;
        const assets::scene_asset_record_t* _scene_record{ nullptr };
        const assets::loaded_tilemap_asset_t* _tilemap{ nullptr };
        const assets::loaded_sprite_asset_t* _player_sprite{ nullptr };
        const assets::loaded_sprite_asset_t* _npc_proof_sprite{ nullptr };
        import::prepared_tilemap_world_data_t _prepared_tilemap_world_data;
        std::shared_ptr<background_prepare_state_t> _background_prepare_state;
        std::optional<std::jthread> _background_prepare_thread;
        world_t _staged_world;
        phase_t _phase{ phase_t::resolve_scene };
    };

    class scene_loader_t
    {
    public:
        [[nodiscard]] static bool load_scene(core::game_context_t& game,
                                            std::string_view scene_id,
                                            std::string_view spawn_marker_override = {});
        [[nodiscard]] static bool load_scene(world_t& world,
                                            assets::asset_manager_t& assets,
                                            std::string_view scene_id,
                                            std::string_view spawn_marker_override = {});
    };
} // namespace carrot::world
