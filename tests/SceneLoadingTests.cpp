//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "Assets/AssetDiscovery.h"
#include "Assets/AssetManager.h"
#include "Assets/Audio/AudioAssetManifestImporter.h"
#include "Assets/Font/FontAssetManifestImporter.h"
#include "Assets/Scene/SceneAssetManifestImporter.h"
#include "Assets/Sprite/SpriteAssetManifestImporter.h"
#include "Assets/Texture/TextureAssetManifestImporter.h"
#include "Assets/Tilemap/TiledTilemapAssetImporter.h"
#include "Assets/Tilemap/TilemapAssetManifestImporter.h"
#include "Assets/Tilemap/TypedObjectConventions.h"
#include "Assets/Tilemap/TilemapValidation.h"
#include "Core/GameRuntime.h"
#include "Core/GameView.h"
#include "EngineConfig.h"
#include "Renderer/Renderer.h"
#include "Scene/Scene.h"
#include "GameplayRuntimeState.h"
#include "IO/VirtualFileSystem.h"
#include "RHI/RHI.h"
#include "RHI/Backends/Null/NullRHIContext.h"
#include "Utils/JSON/Public/JsonDocument.h"
#include "World/AuthoredInteractions.h"
#include "World/Controllers/InteractionController.h"
#include "World/Controllers/PlayerController.h"
#include "World/Import/TilemapWorldBridge.h"
#include "World/SceneContinuity.h"
#include "World/SceneLoader.h"
#include "World/TriggerQuery.h"
#include "World/World.h"
#include "World/WorldLayering.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        [[nodiscard]] std::unique_ptr<rhi::rhi_context_t> make_null_rhi()
        {
            return rhi::create_rhi_context(rhi::rhi_desc_t{
                .api = rhi::graphics_api::null_backend,
                .presentation_window_id = window::invalid_window_id,
                .width = 1280u,
                .height = 720u,
                .enable_debug_layers = false
            });
        }

        class fake_context_t
        {
        public:
            fake_context_t()
                : _context{ make_null_rhi() }
            {
                CARROT_TEST_REQUIRE(_context != nullptr);
            }

            [[nodiscard]] rhi::rhi_context_t& get() noexcept { return *_context; }
            [[nodiscard]] const rhi::rhi_context_t& get() const noexcept { return *_context; }

            operator rhi::rhi_context_t&() noexcept { return *_context; }
            operator const rhi::rhi_context_t&() const noexcept { return *_context; }

        private:
            std::unique_ptr<rhi::rhi_context_t> _context;
        };

        class test_player_controller_t final : public world::player_controller_t
        {
        public:
            void force_apply_animation(world::world_object_t& object,
                                       const world::facing_direction_t facing,
                                       const bool moving)
            {
                apply_animation(object, facing, moving);
            }
        };

        class recording_scene_runtime_listener_t final : public scene::scene_runtime_listener_t
        {
        public:
            void reset() noexcept
            {
                before_call_count = 0u;
                after_call_count = 0u;
                before_had_current_context = false;
                before_current_scene_id.clear();
                before_current_spawn_marker.clear();
                before_next_scene_id.clear();
                before_next_spawn_marker.clear();
                after_scene_id.clear();
                after_spawn_marker.clear();
            }

            void before_scene_change(core::game_context_t&,
                                     const scene::scene_runtime_context_t* current_context,
                                     const std::string_view next_scene_id,
                                     const std::string_view next_spawn_marker) override
            {
                ++before_call_count;
                before_had_current_context = current_context != nullptr;
                before_current_scene_id = current_context ? std::string{ current_context->scene_id } : std::string{};
                before_current_spawn_marker = current_context ? std::string{ current_context->spawn_marker } : std::string{};
                before_next_scene_id = std::string{ next_scene_id };
                before_next_spawn_marker = std::string{ next_spawn_marker };
            }

            void after_scene_change(core::game_context_t&,
                                    const scene::scene_runtime_context_t& current_context) override
            {
                ++after_call_count;
                after_scene_id = std::string{ current_context.scene_id };
                after_spawn_marker = std::string{ current_context.spawn_marker };
            }

            size_t before_call_count{ 0u };
            size_t after_call_count{ 0u };
            bool before_had_current_context{ false };
            std::string before_current_scene_id;
            std::string before_current_spawn_marker;
            std::string before_next_scene_id;
            std::string before_next_spawn_marker;
            std::string after_scene_id;
            std::string after_spawn_marker;
        };

        class post_activation_scene_runtime_listener_t final : public scene::scene_runtime_listener_t
        {
        public:
            void after_scene_change(core::game_context_t& game,
                                    const scene::scene_runtime_context_t& current_context) override
            {
                ++after_call_count;
                after_scene_id = std::string{ current_context.scene_id };
                after_spawn_marker = std::string{ current_context.spawn_marker };

                const world::world_object_t* player{ current_context.player() };
                const world::world_object_t* spawn{ current_context.spawn_object() };
                after_player_name = player ? player->name : std::string{};
                after_spawn_name = spawn ? spawn->name : std::string{};

                after_player_controller_object = player_controller && player_controller->controlled_object()
                                                     ? player_controller->controlled_object()->name
                                                     : std::string{};
                after_interaction_actor_object = interaction_controller && interaction_controller->actor()
                                                     ? interaction_controller->actor()->name
                                                     : std::string{};

                after_player_controller_matches_context_player =
                    player_controller && player && player_controller->controlled_object() == player;
                after_interaction_actor_matches_player =
                    interaction_controller && player && interaction_controller->actor() == player;

                after_camera_center_world = game.view.camera_center_world_position(game.world);
                if (spawn && spawn->transform)
                    expected_camera_center_world = spawn->transform->position;
                else if (player && player->transform)
                    expected_camera_center_world = player->transform->position;
            }

            world::player_controller_t* player_controller{ nullptr };
            world::interaction_controller_t* interaction_controller{ nullptr };
            size_t after_call_count{ 0u };
            std::string after_scene_id;
            std::string after_spawn_marker;
            std::string after_player_name;
            std::string after_spawn_name;
            std::string after_player_controller_object;
            std::string after_interaction_actor_object;
            bool after_player_controller_matches_context_player{ false };
            bool after_interaction_actor_matches_player{ false };
            chlm::float2 after_camera_center_world{ 0.f, 0.f };
            chlm::float2 expected_camera_center_world{ 0.f, 0.f };
        };

        void mount_test_asset_roots(io::virtual_file_system_t& vfs);
        void register_required_assets(assets::asset_manager_t& assets, const io::virtual_file_system_t& vfs);

        void test_scene_load_options_helper_carries_runtime_bindings()
        {
            world::player_controller_t player_controller;
            world::interaction_controller_t interaction_controller;
            recording_scene_runtime_listener_t listener;

            const scene::scene_runtime_bindings_t bindings{
                .player_controller = &player_controller,
                .interaction_controller = &interaction_controller,
                .validate_loaded_scene = nullptr,
                .listener = &listener
            };

            const scene::scene_load_options_t options{
                scene::make_scene_load_options(bindings, "PlayerSpawn")
            };

            CARROT_TEST_REQUIRE(options.spawn_marker_override == "PlayerSpawn");
            CARROT_TEST_REQUIRE(options.player_controller == &player_controller);
            CARROT_TEST_REQUIRE(options.interaction_controller == &interaction_controller);
            CARROT_TEST_REQUIRE(options.listener == &listener);
            CARROT_TEST_REQUIRE(options.validate_loaded_scene == nullptr);
            CARROT_TEST_REQUIRE(options.apply_camera_defaults);
            CARROT_TEST_REQUIRE(options.apply_scene_music);
            CARROT_TEST_REQUIRE(!bindings.empty());
            CARROT_TEST_REQUIRE(scene::scene_runtime_bindings_t{}.empty());
        }

        void test_scene_runtime_default_bindings_apply_to_load_requests()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            world::player_controller_t player_controller;
            world::interaction_controller_t interaction_controller;
            recording_scene_runtime_listener_t listener;
            runtime.set_default_runtime_bindings(scene::scene_runtime_bindings_t{
                .player_controller = &player_controller,
                .interaction_controller = &interaction_controller,
                .listener = &listener
            });

            CARROT_TEST_REQUIRE(runtime.load(game,
                                             "scene.sandbox.town",
                                             scene::scene_load_options_t{
                                                 .apply_scene_music = false,
                                                 .transition_overlay = {
                                                     .style = scene::scene_transition_overlay_style_t::none
                                                 }
                                             }));

            CARROT_TEST_REQUIRE(runtime.default_runtime_bindings().player_controller == &player_controller);
            CARROT_TEST_REQUIRE(runtime.default_runtime_bindings().interaction_controller == &interaction_controller);
            CARROT_TEST_REQUIRE(runtime.default_runtime_bindings().listener == &listener);
            CARROT_TEST_REQUIRE(listener.before_call_count == 1u);
            CARROT_TEST_REQUIRE(listener.after_call_count == 1u);
            CARROT_TEST_REQUIRE(player_controller.controlled_object() != nullptr);
            CARROT_TEST_REQUIRE(interaction_controller.actor() == player_controller.controlled_object());
        }

        void test_scene_runtime_explicit_load_bindings_override_runtime_defaults()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            world::player_controller_t default_player_controller;
            world::interaction_controller_t default_interaction_controller;
            recording_scene_runtime_listener_t default_listener;
            runtime.set_default_runtime_bindings(scene::scene_runtime_bindings_t{
                .player_controller = &default_player_controller,
                .interaction_controller = &default_interaction_controller,
                .listener = &default_listener
            });

            world::player_controller_t override_player_controller;
            world::interaction_controller_t override_interaction_controller;
            recording_scene_runtime_listener_t override_listener;

            CARROT_TEST_REQUIRE(runtime.load(game,
                                             "scene.sandbox.town",
                                             scene::scene_load_options_t{
                                                 .player_controller = &override_player_controller,
                                                 .interaction_controller = &override_interaction_controller,
                                                 .listener = &override_listener,
                                                 .apply_scene_music = false,
                                                 .transition_overlay = {
                                                     .style = scene::scene_transition_overlay_style_t::none
                                                 }
                                             }));

            CARROT_TEST_REQUIRE(default_listener.before_call_count == 0u);
            CARROT_TEST_REQUIRE(default_listener.after_call_count == 0u);
            CARROT_TEST_REQUIRE(override_listener.before_call_count == 1u);
            CARROT_TEST_REQUIRE(override_listener.after_call_count == 1u);
            CARROT_TEST_REQUIRE(override_player_controller.controlled_object() != nullptr);
            CARROT_TEST_REQUIRE(override_interaction_controller.actor() == override_player_controller.controlled_object());
            CARROT_TEST_REQUIRE(default_player_controller.controlled_object() == nullptr);
            CARROT_TEST_REQUIRE(default_interaction_controller.actor() == nullptr);
        }

        void test_game_view_camera_surface_reads_and_writes_camera_state()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };
            world::world_t world;
            core::game_view_t view{ renderer };

            view.set_camera_zoom(3.5f);
            CARROT_TEST_REQUIRE(view.camera_zoom() == 3.5f);

            const core::game_view_camera_t desired{
                .zoom = 2.25f,
                .center_world = { 12.f, 8.f }
            };
            view.set_camera_state(world, desired);

            const core::game_view_camera_t actual{ view.camera_state(world) };
            CARROT_TEST_REQUIRE(actual.zoom == desired.zoom);
            CARROT_TEST_REQUIRE(actual.center_world.x == desired.center_world.x);
            CARROT_TEST_REQUIRE(actual.center_world.y == desired.center_world.y);

            const chlm::float2 center{ view.camera_center_world_position(world) };
            CARROT_TEST_REQUIRE(center.x == desired.center_world.x);
            CARROT_TEST_REQUIRE(center.y == desired.center_world.y);
        }

        bool g_rebuild_validation_should_fail{ false };
        size_t g_rebuild_validation_call_count{ 0u };

        bool rebuild_validation_callback(const assets::asset_manager_t&,
                                         const world::world_t&,
                                         const std::string_view)
        {
            ++g_rebuild_validation_call_count;
            return !g_rebuild_validation_should_fail;
        }

        [[nodiscard]] std::filesystem::path game_assets_root()
        {
            return std::filesystem::path{ CARROT_SOURCE_ROOT } / "src" / "Sandbox" / "assets";
        }

        [[nodiscard]] std::filesystem::path engine_assets_root()
        {
            return std::filesystem::path{ CARROT_SOURCE_ROOT } / "assets";
        }

        void mount_test_asset_roots(io::virtual_file_system_t& vfs)
        {
            vfs.mount("engine", engine_assets_root(), true);
            vfs.mount("game", game_assets_root(), true);
        }

        [[nodiscard]] utils::json::json_document_t parse_json(const io::virtual_file_system_t& vfs, std::string_view manifest_uri)
        {
            const auto native_path{ vfs.resolve_native_path(manifest_uri) };
            CARROT_TEST_REQUIRE(native_path.has_value());

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_file(native_path->string().c_str()));
            return doc;
        }

        void register_required_assets(assets::asset_manager_t& assets, const io::virtual_file_system_t& vfs)
        {
            const auto manifests{ assets::asset_discovery_t::discover_supported_manifests(vfs) };

            for (const std::string& manifest : manifests.audio)
            {
                utils::json::json_document_t doc{ parse_json(vfs, manifest) };
                CARROT_TEST_REQUIRE(assets::audio_asset_manifest_importer_t::import(doc, assets.audio().registry(), vfs));
            }

            for (const std::string& manifest : manifests.fonts)
            {
                utils::json::json_document_t doc{ parse_json(vfs, manifest) };
                CARROT_TEST_REQUIRE(assets::font_asset_manifest_importer_t::import(doc, assets.fonts().registry(), vfs, manifest));
            }

            for (const std::string& manifest : manifests.textures)
            {
                utils::json::json_document_t doc{ parse_json(vfs, manifest) };
                CARROT_TEST_REQUIRE(assets::texture_asset_manifest_importer_t::import(doc, assets.textures().registry(), vfs));
            }

            for (const std::string& manifest : manifests.sprites)
            {
                utils::json::json_document_t doc{ parse_json(vfs, manifest) };
                CARROT_TEST_REQUIRE(assets::sprite_asset_manifest_importer_t::import(doc, assets.sprites().registry(), vfs));
            }

            for (const std::string& manifest : manifests.tilemaps)
            {
                utils::json::json_document_t doc{ parse_json(vfs, manifest) };
                CARROT_TEST_REQUIRE(assets::tilemap_asset_manifest_importer_t::import(doc, assets.tilemaps().registry(), vfs));
            }

            for (const std::string& manifest : manifests.scenes)
            {
                utils::json::json_document_t doc{ parse_json(vfs, manifest) };
                CARROT_TEST_REQUIRE(assets::scene_asset_manifest_importer_t::import(doc, assets.scenes().registry(), vfs, manifest));

                const utils::json::json_object_view_t root{ doc.root().as_object() };
                const std::string_view scene_id{ root.get_string("id") };
                const assets::scene_asset_record_t* scene{ assets.scenes().registry().find(scene_id) };
                CARROT_TEST_REQUIRE(scene != nullptr);
                CARROT_TEST_REQUIRE(assets.scenes().registry().validate_references(*scene,
                                                                                   assets.tilemaps().registry(),
                                                                                   assets.sprites().registry(),
                                                                                   assets.audio().registry()));
            }
        }

        void test_asset_discovery_finds_scene_manifest()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const auto manifests{ assets::asset_discovery_t::discover_supported_manifests(vfs) };
            CARROT_TEST_REQUIRE(!manifests.scenes.empty());

            const auto it{ std::find(manifests.scenes.begin(), manifests.scenes.end(), "game://scenes/test_overworld.scene.json") };
            CARROT_TEST_REQUIRE(it != manifests.scenes.end());
        }

        void test_asset_discovery_skips_missing_mount_root_without_throwing()
        {
            io::virtual_file_system_t vfs;
            const std::filesystem::path missing_root{
                std::filesystem::temp_directory_path() / "carrot_missing_mount_root_for_tests"
            };
            std::filesystem::remove_all(missing_root);
            vfs.mount("game", missing_root, true);

            bool threw{ false };
            assets::discovered_asset_manifests_t manifests;

            try
            {
                manifests = assets::asset_discovery_t::discover_supported_manifests(vfs);
            }
            catch (const std::filesystem::filesystem_error&)
            {
                threw = true;
            }

            CARROT_TEST_REQUIRE(!threw);
            CARROT_TEST_REQUIRE(manifests.total_count() == 0u);
        }

        void test_asset_discovery_skips_file_mount_root_without_throwing()
        {
            io::virtual_file_system_t vfs;
            const std::filesystem::path temp_root{
                std::filesystem::temp_directory_path() / "carrot_asset_discovery_file_mount_root"
            };
            std::filesystem::create_directories(temp_root);
            const std::filesystem::path file_root{ temp_root / "not_a_directory.txt" };
            {
                std::ofstream out{ file_root };
                out << "carrot";
            }
            vfs.mount("game", file_root, true);

            bool threw{ false };
            assets::discovered_asset_manifests_t manifests;

            try
            {
                manifests = assets::asset_discovery_t::discover_supported_manifests(vfs);
            }
            catch (const std::filesystem::filesystem_error&)
            {
                threw = true;
            }

            std::filesystem::remove_all(temp_root);

            CARROT_TEST_REQUIRE(!threw);
            CARROT_TEST_REQUIRE(manifests.total_count() == 0u);
        }

        void test_tilemap_world_bridge_imports_authored_objects()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.test.overworld") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);

            world::world_t world;
            const world::import::tilemap_world_bridge_result_t result{
                world::import::import_tilemap_objects(world, *tilemap)
            };

            CARROT_TEST_REQUIRE(result.markers_created >= 1);
            CARROT_TEST_REQUIRE(result.tile_objects_created >= 1);
            CARROT_TEST_REQUIRE(world.find_object_by_name("PlayerSpawn") != nullptr);
            CARROT_TEST_REQUIRE(world.find_object_by_name("StarterChest") != nullptr);
            CARROT_TEST_REQUIRE(world.find_first_object_by_type("Door") != nullptr);
        }

        void test_tilemap_world_bridge_imports_tileset_collision_as_static_colliders()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.sandbox.town") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);

            bool found_tileset_collision_metadata{ false };
            for (const assets::tilemap_tileset_t& tileset : tilemap->tilemap().tilesets())
            {
                if (!tileset.tile_collisions.empty())
                {
                    found_tileset_collision_metadata = true;
                    break;
                }
            }

            CARROT_TEST_REQUIRE(found_tileset_collision_metadata);

            world::world_t world;
            const world::import::tilemap_world_bridge_result_t result{
                world::import::import_tilemap_objects(world, *tilemap)
            };

            CARROT_TEST_REQUIRE(result.static_colliders_created > 0u);
            CARROT_TEST_REQUIRE(world.collision_world().static_colliders().size() == result.static_colliders_created);

            const auto& first_collider{ world.collision_world().static_colliders().front() };
            const auto hits{ world.collision_world().point_query(first_collider.bounds.center()) };
            CARROT_TEST_REQUIRE(!hits.empty());
        }

        void test_tilemap_world_bridge_imports_object_layer_tile_collision_as_static_collider()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.sandbox.town") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);

            const assets::tilemap_object_t* hay_object{ nullptr };
            const assets::tilemap_tileset_t* hay_tileset{ nullptr };
            for (const assets::tilemap_layer_t& layer : tilemap->tilemap().layers())
            {
                if (layer.kind != assets::tilemap_layer_kind_t::object)
                    continue;

                for (const assets::tilemap_object_t& object : layer.objects)
                {
                    if (object.name != "hay" || object.gid == 0)
                        continue;

                    hay_object = &object;

                    for (const assets::tilemap_tileset_t& tileset : tilemap->tilemap().tilesets())
                    {
                        const uint32_t next_first_gid{
                            (&tileset + 1) < (tilemap->tilemap().tilesets().data() + tilemap->tilemap().tilesets().size())
                                ? (&tileset + 1)->first_gid
                                : std::numeric_limits<uint32_t>::max()
                        };

                        if (object.gid >= tileset.first_gid && object.gid < next_first_gid)
                        {
                            hay_tileset = &tileset;
                            break;
                        }
                    }

                    break;
                }

                if (hay_object != nullptr)
                    break;
            }

            CARROT_TEST_REQUIRE(hay_object != nullptr);
            CARROT_TEST_REQUIRE(hay_tileset != nullptr);
            CARROT_TEST_REQUIRE(hay_tileset->find_tile_collision(hay_object->gid - hay_tileset->first_gid) != nullptr);

            world::world_t world;
            const world::import::tilemap_world_bridge_result_t result{
                world::import::import_tilemap_objects(world, *tilemap)
            };
            CARROT_TEST_REQUIRE(result.static_colliders_created > 0u);

            const world::world_object_t* hay_world_object{ world.find_object_by_name("hay") };
            CARROT_TEST_REQUIRE(hay_world_object != nullptr);
            CARROT_TEST_REQUIRE(hay_world_object->transform.has_value());

            const collision::collision_aabb_t hay_bounds{ collision::collision_aabb_t::from_min_size(
                hay_world_object->transform->position,
                chlm::float2{
                    world::world_units_t::pixels_to_world(hay_object->width),
                    world::world_units_t::pixels_to_world(hay_object->height)
                }) };
            const auto overlaps{ world.collision_world().overlap_query(hay_bounds) };
            CARROT_TEST_REQUIRE(!overlaps.empty());
        }

        void test_tilemap_world_bridge_imports_authored_triggers()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.sandbox.town") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);

            world::world_t world;
            const world::import::tilemap_world_bridge_result_t result{
                world::import::import_tilemap_objects(world, *tilemap)
            };

            CARROT_TEST_REQUIRE(result.triggers_created > 0u);

            world::world_object_t* trigger{ world.find_object_by_name("Trigger") };
            CARROT_TEST_REQUIRE(trigger != nullptr);
            CARROT_TEST_REQUIRE(trigger->trigger.has_value());
            CARROT_TEST_REQUIRE(trigger->collision.has_value());
            CARROT_TEST_REQUIRE(trigger->trigger->trigger_id == "inn_trigger_1");
            CARROT_TEST_REQUIRE(trigger->trigger->trigger_kind == "unlock_quest");
        }

        void test_tilemap_world_bridge_imports_visibility_regions()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.sandbox.town") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);

            world::world_t world;
            const world::import::tilemap_world_bridge_result_t result{
                world::import::import_tilemap_objects(world, *tilemap)
            };

            CARROT_TEST_REQUIRE(result.markers_created > 0u);

            const world::world_object_t* inn_region{ nullptr };
            const world::world_object_t* shop_region{ nullptr };
            for (const world::world_object_t& object : world.objects())
            {
                if (!object.visibility_region)
                    continue;

                if (object.visibility_region->tag == "inn_roof")
                    inn_region = &object;
                else if (object.visibility_region->tag == "item_shop_roof")
                    shop_region = &object;
            }
            CARROT_TEST_REQUIRE(inn_region != nullptr);
            CARROT_TEST_REQUIRE(shop_region != nullptr);
            CARROT_TEST_REQUIRE(inn_region->transform.has_value());
            CARROT_TEST_REQUIRE(shop_region->transform.has_value());
            CARROT_TEST_REQUIRE(inn_region->visibility_region.has_value());
            CARROT_TEST_REQUIRE(shop_region->visibility_region.has_value());
            CARROT_TEST_REQUIRE(inn_region->visibility_region->tag == "inn_roof");
            CARROT_TEST_REQUIRE(shop_region->visibility_region->tag == "item_shop_roof");

            const auto active_tags{ world.collect_active_visibility_tags({
                inn_region->transform->position.x + 0.5f,
                inn_region->transform->position.y + 0.5f
            }) };
            CARROT_TEST_REQUIRE(std::ranges::find(active_tags, std::string_view{ "inn_roof" }) != active_tags.end());

            const auto active_shop_tags{ world.collect_active_visibility_tags({
                shop_region->transform->position.x + 0.5f,
                shop_region->transform->position.y + 0.5f
            }) };
            CARROT_TEST_REQUIRE(std::ranges::find(active_shop_tags, std::string_view{ "item_shop_roof" }) != active_shop_tags.end());

            const auto inactive_tags{ world.collect_active_visibility_tags({ 2.f, 2.f }) };
            CARROT_TEST_REQUIRE(std::ranges::find(inactive_tags, std::string_view{ "inn_roof" }) == inactive_tags.end());
            CARROT_TEST_REQUIRE(std::ranges::find(inactive_tags, std::string_view{ "item_shop_roof" }) == inactive_tags.end());
        }

        void test_tiled_group_visibility_zone_properties_flow_to_child_layers()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.sandbox.town") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);

            bool found_inn_roof_layer{ false };
            bool found_item_shop_roof_layer{ false };

            for (const assets::tilemap_layer_t& layer : tilemap->tilemap().layers())
            {
                const auto zone_id{ layer.get_string_property("visibility_zone_id") };
                if (!zone_id.has_value())
                    continue;

                found_inn_roof_layer = found_inn_roof_layer || (*zone_id == "inn_roof");
                found_item_shop_roof_layer = found_item_shop_roof_layer || (*zone_id == "item_shop_roof");
            }

            CARROT_TEST_REQUIRE(found_inn_roof_layer);
            CARROT_TEST_REQUIRE(found_item_shop_roof_layer);
        }

        void test_tiled_nested_group_properties_flow_to_child_layers()
        {
            constexpr std::string_view json_source{ R"json(
                {
                  "type": "map",
                  "orientation": "orthogonal",
                  "width": 4,
                  "height": 4,
                  "tilewidth": 16,
                  "tileheight": 16,
                  "layers": [
                    {
                      "type": "group",
                      "name": "Outer",
                      "visible": false,
                      "properties": [
                        { "name": "visibility_zone_id", "type": "string", "value": "outer_zone" }
                      ],
                      "layers": [
                        {
                          "type": "group",
                          "name": "Inner",
                          "properties": [
                            { "name": "carrot_conditional_front", "type": "bool", "value": true }
                          ],
                          "layers": [
                            {
                              "type": "tilelayer",
                              "name": "NestedRoof",
                              "width": 4,
                              "height": 4,
                              "visible": true,
                              "opacity": 1,
                              "data": [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
                            }
                          ]
                        }
                      ]
                    }
                  ],
                  "tilesets": []
                }
            )json" };

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_memory(json_source.data(), json_source.size()));

            assets::tilemap_asset_registry_t registry;
            CARROT_TEST_REQUIRE(assets::tiled_tilemap_asset_importer_t::import(doc,
                                                                               registry,
                                                                               "tilemap.test.nested_groups",
                                                                               "game://tilemaps/test_nested_groups.tmj"));

            const assets::tilemap_asset_record_t* record{ registry.find("tilemap.test.nested_groups") };
            CARROT_TEST_REQUIRE(record != nullptr);
            CARROT_TEST_REQUIRE(record->tilemap.layers().size() == 1u);

            const assets::tilemap_layer_t& layer{ record->tilemap.layers().front() };
            CARROT_TEST_REQUIRE(layer.name == "NestedRoof");
            CARROT_TEST_REQUIRE(!layer.visible);
            CARROT_TEST_REQUIRE(layer.get_string_property("visibility_zone_id").value_or("") == "outer_zone");
            CARROT_TEST_REQUIRE(layer.get_bool_property("carrot_conditional_front").value_or(false));
        }

        void test_world_layering_uses_explicit_visibility_zones()
        {
            assets::tilemap_layer_t roof_layer{
                .kind = assets::tilemap_layer_kind_t::tile,
                .name = "AnyLayerName",
                .authored_type = "Roof"
            };
            roof_layer.properties.push_back(assets::tilemap_property_t{
                .name = "visibility_zone_id",
                .value = std::string{ "inn_roof" }
            });

            const world::authored_layer_semantics_t roof_semantics{
                world::resolve_tile_layer_semantics(roof_layer, 13)
            };
            CARROT_TEST_REQUIRE(roof_semantics.render_layer == renderer::render_layer_t::world_front);
            CARROT_TEST_REQUIRE(roof_semantics.visibility_tag == "inn_roof");
            CARROT_TEST_REQUIRE(roof_semantics.visibility_rule == world::layer_visibility_rule_t::hidden_when_tag_active);
            CARROT_TEST_REQUIRE(!world::is_layer_visible(roof_semantics, std::array<std::string_view, 1>{ "inn_roof" }));
        }

        void test_tiled_layer_front_properties_import_from_latest_town_map()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.sandbox.town") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);

            const assets::tilemap_layer_t* props1{ nullptr };
            const assets::tilemap_layer_t* props2{ nullptr };
            const assets::tilemap_layer_t* foreground_props{ nullptr };

            for (const assets::tilemap_layer_t& layer : tilemap->tilemap().layers())
            {
                if (layer.name == "props1")
                    props1 = &layer;
                else if (layer.name == "props2")
                    props2 = &layer;
                else if (layer.name == "foreground_props")
                    foreground_props = &layer;
            }

            CARROT_TEST_REQUIRE(props1 != nullptr);
            CARROT_TEST_REQUIRE(props2 != nullptr);
            CARROT_TEST_REQUIRE(foreground_props != nullptr);
            CARROT_TEST_REQUIRE(props1->get_bool_property("carrot_conditional_front").value_or(false));
            CARROT_TEST_REQUIRE(props2->get_bool_property("carrot_conditional_front").value_or(false));
            CARROT_TEST_REQUIRE(foreground_props->get_bool_property("carrot_always_front").value_or(false));
        }

        void test_world_layering_resolves_conditional_and_always_front()
        {
            assets::tilemap_layer_t conditional_layer{
                .kind = assets::tilemap_layer_kind_t::tile,
                .name = "props1"
            };
            conditional_layer.properties.push_back(assets::tilemap_property_t{
                .name = "carrot_conditional_front",
                .value = true
            });

            const world::authored_layer_semantics_t conditional_semantics{
                world::resolve_tile_layer_semantics(conditional_layer, 20)
            };
            CARROT_TEST_REQUIRE(conditional_semantics.render_layer == renderer::render_layer_t::actors);
            CARROT_TEST_REQUIRE(conditional_semantics.order_mode == renderer::render_order_mode_t::anchor_bottom_y);

            assets::tilemap_layer_t always_front_layer{
                .kind = assets::tilemap_layer_kind_t::tile,
                .name = "foreground_props"
            };
            always_front_layer.properties.push_back(assets::tilemap_property_t{
                .name = "carrot_always_front",
                .value = true
            });

            const world::authored_layer_semantics_t always_front_semantics{
                world::resolve_tile_layer_semantics(always_front_layer, 21)
            };
            CARROT_TEST_REQUIRE(always_front_semantics.render_layer == renderer::render_layer_t::world_front);
            CARROT_TEST_REQUIRE(always_front_semantics.order_mode == renderer::render_order_mode_t::explicit_order);

            assets::tilemap_layer_t precedence_layer{
                .kind = assets::tilemap_layer_kind_t::tile,
                .name = "foreground_props"
            };
            precedence_layer.properties.push_back(assets::tilemap_property_t{
                .name = "carrot_conditional_front",
                .value = true
            });
            precedence_layer.properties.push_back(assets::tilemap_property_t{
                .name = "carrot_always_front",
                .value = true
            });

            const world::authored_layer_semantics_t precedence_semantics{
                world::resolve_tile_layer_semantics(precedence_layer, 22)
            };
            CARROT_TEST_REQUIRE(precedence_semantics.render_layer == renderer::render_layer_t::world_front);
            CARROT_TEST_REQUIRE(precedence_semantics.order_mode == renderer::render_order_mode_t::explicit_order);
        }

        void test_world_layering_does_not_force_waterfall_layers_into_background()
        {
            assets::tilemap_layer_t waterfall_layer{
                .kind = assets::tilemap_layer_kind_t::tile,
                .name = "waterfall",
                .authored_type = "Waterfall"
            };

            const world::authored_layer_semantics_t waterfall_semantics{
                world::resolve_tile_layer_semantics(waterfall_layer, 8)
            };

            CARROT_TEST_REQUIRE(waterfall_semantics.render_layer == renderer::render_layer_t::world_back);

            assets::tilemap_layer_t water_layer{
                .kind = assets::tilemap_layer_kind_t::tile,
                .name = "anything",
                .authored_type = "Water"
            };

            const world::authored_layer_semantics_t water_semantics{
                world::resolve_tile_layer_semantics(water_layer, 0)
            };

            CARROT_TEST_REQUIRE(water_semantics.render_layer == renderer::render_layer_t::background);
        }

        void test_world_layering_debug_snapshot_round_trips_through_world()
        {
            world::world_t world;

            world::layering_debug_snapshot_t snapshot{ };
            snapshot.frame_index = 42;
            snapshot.has_visibility_anchor = true;
            snapshot.visibility_anchor_world = { 12.f, 18.f };
            snapshot.visibility_region_count = 3;
            snapshot.rendered_tilemap_count = 1;
            snapshot.layer_count = 4;
            snapshot.visible_layer_count = 3;
            snapshot.hidden_layer_count = 1;
            snapshot.visibility_bound_layer_count = 2;
            snapshot.conditional_front_layer_count = 1;
            snapshot.always_front_layer_count = 1;
            snapshot.active_visibility_tags.emplace_back("inn_roof");
            snapshot.layers.push_back({
                .layer_name = "props1",
                .source_kind = "tile",
                .resolved_render_layer = renderer::render_layer_t::actors,
                .resolved_order_mode = renderer::render_order_mode_t::anchor_bottom_y,
                .resolved_order_in_layer = 120,
                .visibility_zone_id = "inn_roof",
                .is_visible = false,
                .hidden_by_visibility_zone = true,
                .is_conditional_front = true,
                .is_always_front = false
            });

            world.set_layering_debug_snapshot(std::move(snapshot));

            const world::layering_debug_snapshot_t& stored{ world.layering_debug_snapshot() };
            CARROT_TEST_REQUIRE(stored.frame_index == 42);
            CARROT_TEST_REQUIRE(stored.has_visibility_anchor);
            CARROT_TEST_REQUIRE(stored.visibility_anchor_world.x == 12.f);
            CARROT_TEST_REQUIRE(stored.visibility_anchor_world.y == 18.f);
            CARROT_TEST_REQUIRE(stored.visibility_region_count == 3u);
            CARROT_TEST_REQUIRE(stored.rendered_tilemap_count == 1u);
            CARROT_TEST_REQUIRE(stored.hidden_layer_count == 1u);
            CARROT_TEST_REQUIRE(stored.conditional_front_layer_count == 1u);
            CARROT_TEST_REQUIRE(stored.always_front_layer_count == 1u);
            CARROT_TEST_REQUIRE(stored.active_visibility_tags.size() == 1u);
            CARROT_TEST_REQUIRE(stored.active_visibility_tags.front() == "inn_roof");
            CARROT_TEST_REQUIRE(stored.layers.size() == 1u);
            CARROT_TEST_REQUIRE(stored.layers.front().layer_name == "props1");
            CARROT_TEST_REQUIRE(stored.layers.front().hidden_by_visibility_zone);
            CARROT_TEST_REQUIRE(stored.layers.front().is_conditional_front);
        }

        void test_tiled_authored_data_validation_reports_visibility_zone_contract_issues()
        {
            assets::tilemap_asset_t tilemap;

            assets::tilemap_layer_t roof_layer{
                .kind = assets::tilemap_layer_kind_t::tile,
                .name = "RoofFront"
            };
            roof_layer.properties.push_back(assets::tilemap_property_t{
                .name = "carrot_conditional_front",
                .value = true
            });
            roof_layer.properties.push_back(assets::tilemap_property_t{
                .name = "carrot_always_front",
                .value = true
            });
            roof_layer.properties.push_back(assets::tilemap_property_t{
                .name = "carrot_visibility_zone",
                .value = std::string{ "roof_a" }
            });
            roof_layer.properties.push_back(assets::tilemap_property_t{
                .name = "visibility_zone_id",
                .value = std::string{ "roof_b" }
            });
            tilemap.add_layer(std::move(roof_layer));

            assets::tilemap_layer_t markers_layer{
                .kind = assets::tilemap_layer_kind_t::object,
                .name = "Markers"
            };

            assets::tilemap_object_t wrong_type_zone{ };
            wrong_type_zone.name = "BadZone";
            wrong_type_zone.type = "Marker";
            wrong_type_zone.width = 32.f;
            wrong_type_zone.height = 32.f;
            wrong_type_zone.properties.push_back(assets::tilemap_property_t{
                .name = "visibility_zone_id",
                .value = std::string{ "roof_a" }
            });
            markers_layer.objects.push_back(std::move(wrong_type_zone));

            assets::tilemap_object_t empty_visibility_zone{ };
            empty_visibility_zone.name = "EmptyZone";
            empty_visibility_zone.type = "VisibilityZone";
            markers_layer.objects.push_back(std::move(empty_visibility_zone));

            tilemap.add_layer(std::move(markers_layer));

            const auto issues{ assets::validate_tiled_authored_data(tilemap) };
            CARROT_TEST_REQUIRE(issues.size() >= 5u);

            const auto has_issue_code{ [&issues](const std::string_view code) {
                return std::ranges::any_of(issues, [code](const assets::tilemap_validation_issue_t& issue) {
                    return issue.code == code;
                });
            } };

            CARROT_TEST_REQUIRE(has_issue_code("tiled.visibility_zone.property_without_type"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.visibility_zone.missing_id"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.visibility_zone.zero_size"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.layer.front_policy_conflict"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.layer.visibility_zone_override_conflict"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.layer.visibility_zone_without_matching_object"));
        }

        void test_imported_sandbox_town_has_no_tiled_authored_data_validation_issues()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.sandbox.town") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);
            CARROT_TEST_REQUIRE(tilemap->tilemap().validation_issues().empty());
        }

        void test_tiled_point_objects_import_as_explicit_point_geometry()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.sandbox.town") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);

            const assets::tilemap_object_t* player_spawn{ tilemap->find_object_by_name("PlayerSpawn") };
            const assets::tilemap_object_t* door_to_inn{ tilemap->find_object_by_name("DoorToInn") };
            CARROT_TEST_REQUIRE(player_spawn != nullptr);
            CARROT_TEST_REQUIRE(door_to_inn != nullptr);
            CARROT_TEST_REQUIRE(player_spawn->geometry_kind == assets::tilemap_object_t::geometry_kind_t::point);
            CARROT_TEST_REQUIRE(player_spawn->width == 0.f);
            CARROT_TEST_REQUIRE(player_spawn->height == 0.f);
            CARROT_TEST_REQUIRE(door_to_inn->geometry_kind == assets::tilemap_object_t::geometry_kind_t::rectangle);
        }

        void test_tiled_polygon_geometry_parses_into_object_metadata()
        {
            constexpr std::string_view json_source{ R"json(
{
  "height": 1,
  "width": 1,
  "tilewidth": 32,
  "tileheight": 32,
  "orientation": "orthogonal",
  "type": "map",
  "layers": [
    {
      "id": 1,
      "name": "Markers",
      "type": "objectgroup",
      "objects": [
        {
          "id": 1,
          "name": "PathA",
          "type": "Path",
          "x": 16,
          "y": 32,
          "polyline": [
            { "x": 0, "y": 0 },
            { "x": 10, "y": -4 },
            { "x": 30, "y": 8 }
          ]
        },
        {
          "id": 2,
          "name": "RegionA",
          "type": "Region",
          "x": 4,
          "y": 5,
          "polygon": [
            { "x": 0, "y": 0 },
            { "x": 12, "y": 0 },
            { "x": 12, "y": 16 }
          ]
        }
      ]
    }
  ],
  "tilesets": []
}
)json" };

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_memory(json_source.data(), json_source.size()));

            assets::tilemap_asset_registry_t registry;
            CARROT_TEST_REQUIRE(assets::tiled_tilemap_asset_importer_t::import(doc, registry, "tilemap.test.geometry", "game://tilemaps/test_geometry.tmj"));

            const assets::tilemap_asset_record_t* record{ registry.find("tilemap.test.geometry") };
            CARROT_TEST_REQUIRE(record != nullptr);

            const assets::tilemap_layer_t& layer{ record->tilemap.layers().front() };
            CARROT_TEST_REQUIRE(layer.objects.size() == 2u);
            CARROT_TEST_REQUIRE(layer.objects[0].geometry_kind == assets::tilemap_object_t::geometry_kind_t::polyline);
            CARROT_TEST_REQUIRE(layer.objects[0].geometry_points.size() == 3u);
            CARROT_TEST_REQUIRE(layer.objects[1].geometry_kind == assets::tilemap_object_t::geometry_kind_t::polygon);
            CARROT_TEST_REQUIRE(layer.objects[1].geometry_points.size() == 3u);
        }

        void test_typed_object_conventions_parse_current_sandbox_objects()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.sandbox.town") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);

            const assets::tilemap_object_t* sign{ tilemap->find_object_by_name("WelcomeSign") };
            const assets::tilemap_object_t* chest{ tilemap->find_object_by_name("StarterChest") };
            const assets::tilemap_object_t* door{ tilemap->find_object_by_name("DoorToInn") };
            const assets::tilemap_object_t* trigger{ tilemap->find_object_by_name("Trigger") };
            const assets::tilemap_object_t* visibility_zone{ tilemap->find_object_by_name("InnRoofTrigger1") };
            const assets::tilemap_object_t* ambient_light{ tilemap->find_object_by_name("AmbientLight") };
            const assets::tilemap_object_t* follow_light{ tilemap->find_object_by_name("FollowLight") };
            CARROT_TEST_REQUIRE(sign != nullptr);
            CARROT_TEST_REQUIRE(chest != nullptr);
            CARROT_TEST_REQUIRE(door != nullptr);
            CARROT_TEST_REQUIRE(trigger != nullptr);
            CARROT_TEST_REQUIRE(visibility_zone != nullptr);
            CARROT_TEST_REQUIRE(ambient_light != nullptr);
            CARROT_TEST_REQUIRE(follow_light != nullptr);

            const auto typed_sign{ assets::as_typed_sign(*sign) };
            const auto typed_container{ assets::as_typed_container(*chest) };
            const auto typed_door{ assets::as_typed_door(*door) };
            const auto typed_trigger{ assets::as_typed_trigger(*trigger) };
            const auto typed_visibility_zone{ assets::as_typed_visibility_zone(*visibility_zone) };
            const auto typed_ambient_light{ assets::as_typed_light(*ambient_light) };
            const auto typed_follow_light{ assets::as_typed_light(*follow_light) };
            CARROT_TEST_REQUIRE(typed_sign.has_value());
            CARROT_TEST_REQUIRE(typed_container.has_value());
            CARROT_TEST_REQUIRE(typed_door.has_value());
            CARROT_TEST_REQUIRE(typed_trigger.has_value());
            CARROT_TEST_REQUIRE(typed_visibility_zone.has_value());
            CARROT_TEST_REQUIRE(typed_ambient_light.has_value());
            CARROT_TEST_REQUIRE(typed_follow_light.has_value());
            CARROT_TEST_REQUIRE(typed_sign->message_id == "sign.welcome");
            CARROT_TEST_REQUIRE(typed_container->loot_table == "starter_chest");
            CARROT_TEST_REQUIRE(typed_door->target_scene == "scene.sandbox.inn");
            CARROT_TEST_REQUIRE(typed_door->target_marker == "EntryFromTown");
            CARROT_TEST_REQUIRE(typed_trigger->trigger_id == "inn_trigger_1");
            CARROT_TEST_REQUIRE(typed_trigger->trigger_kind == "unlock_quest");
            CARROT_TEST_REQUIRE(typed_visibility_zone->visibility_zone_id == "inn_roof");
            CARROT_TEST_REQUIRE(typed_ambient_light->kind == assets::typed_light_kind_t::ambient);
            CARROT_TEST_REQUIRE(typed_ambient_light->behavior == assets::typed_light_behavior_t::stationary);
            CARROT_TEST_REQUIRE(typed_ambient_light->color_hex == "#5C6170");
            CARROT_TEST_REQUIRE(typed_ambient_light->intensity == 1.0f);
            CARROT_TEST_REQUIRE(!typed_ambient_light->radius_world.has_value());
            CARROT_TEST_REQUIRE(typed_follow_light->kind == assets::typed_light_kind_t::point);
            CARROT_TEST_REQUIRE(typed_follow_light->behavior == assets::typed_light_behavior_t::follow);
            CARROT_TEST_REQUIRE(typed_follow_light->follow_target == "player");
            CARROT_TEST_REQUIRE(typed_follow_light->color_hex == "#FFD194");
            CARROT_TEST_REQUIRE(typed_follow_light->radius_world.has_value());
            CARROT_TEST_REQUIRE(*typed_follow_light->radius_world == 3.25f);
        }

        void test_tiled_authored_data_validation_reports_typed_object_contract_issues()
        {
            assets::tilemap_asset_t tilemap;
            assets::tilemap_layer_t markers_layer{
                .kind = assets::tilemap_layer_kind_t::object,
                .name = "Markers"
            };

            assets::tilemap_object_t bad_sign{ };
            bad_sign.name = "BadSign";
            bad_sign.type = "Sign";
            markers_layer.objects.push_back(std::move(bad_sign));

            assets::tilemap_object_t bad_chest{ };
            bad_chest.name = "BadChest";
            bad_chest.type = "Container";
            markers_layer.objects.push_back(std::move(bad_chest));

            assets::tilemap_object_t bad_door{ };
            bad_door.name = "BadDoor";
            bad_door.type = "Door";
            bad_door.properties.push_back({
                .name = "target_scene",
                .value = std::string{ "scene.test" }
            });
            markers_layer.objects.push_back(std::move(bad_door));

            assets::tilemap_object_t mixed_door{ };
            mixed_door.name = "MixedDoor";
            mixed_door.type = "Door";
            mixed_door.properties.push_back({
                .name = "target_scene",
                .value = std::string{ "scene.test" }
            });
            mixed_door.properties.push_back({
                .name = "target_map",
                .value = std::string{ "tilemap.test" }
            });
            mixed_door.properties.push_back({
                .name = "target_marker",
                .value = std::string{ "Spawn" }
            });
            markers_layer.objects.push_back(std::move(mixed_door));

            assets::tilemap_object_t bad_trigger{ };
            bad_trigger.name = "BadTrigger";
            bad_trigger.type = "Trigger";
            bad_trigger.properties.push_back({
                .name = "trigger_id",
                .value = std::string{ "quest.a" }
            });
            markers_layer.objects.push_back(std::move(bad_trigger));

            assets::tilemap_object_t unknown_object{ };
            unknown_object.name = "MysteryThing";
            unknown_object.type = "MysteryThing";
            markers_layer.objects.push_back(std::move(unknown_object));

            tilemap.add_layer(std::move(markers_layer));

            const auto issues{ assets::validate_tiled_authored_data(tilemap) };
            const auto has_issue_code{ [&issues](const std::string_view code) {
                return std::ranges::any_of(issues, [code](const assets::tilemap_validation_issue_t& issue) {
                    return issue.code == code;
                });
            } };

            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.sign.missing_message_id"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.container.missing_loot_table"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.door.invalid_target"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.door.mixed_target_modes"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.trigger.missing_fields"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.unknown_type"));
        }

        void test_tiled_authored_data_validation_reports_light_contract_issues()
        {
            assets::tilemap_asset_t tilemap;
            assets::tilemap_layer_t markers_layer{
                .kind = assets::tilemap_layer_kind_t::object,
                .name = "Markers"
            };

            assets::tilemap_object_t missing_kind{ };
            missing_kind.name = "MissingKindLight";
            missing_kind.type = "Light";
            markers_layer.objects.push_back(std::move(missing_kind));

            assets::tilemap_object_t bad_point{ };
            bad_point.name = "BadPointLight";
            bad_point.type = "Light";
            bad_point.properties.push_back({ .name = "kind", .value = std::string{ "point" } });
            bad_point.properties.push_back({ .name = "color", .value = std::string{ "#GGGGGG" } });
            markers_layer.objects.push_back(std::move(bad_point));

            assets::tilemap_object_t bad_follow{ };
            bad_follow.name = "BadFollowLight";
            bad_follow.type = "Light";
            bad_follow.properties.push_back({ .name = "kind", .value = std::string{ "point" } });
            bad_follow.properties.push_back({ .name = "behavior", .value = std::string{ "follow" } });
            bad_follow.properties.push_back({ .name = "radius", .value = 2.0 });
            markers_layer.objects.push_back(std::move(bad_follow));

            assets::tilemap_object_t ambient_with_follow_target{ };
            ambient_with_follow_target.name = "AmbientWithFollowTarget";
            ambient_with_follow_target.type = "Light";
            ambient_with_follow_target.properties.push_back({ .name = "kind", .value = std::string{ "ambient" } });
            ambient_with_follow_target.properties.push_back({ .name = "behavior", .value = std::string{ "follow" } });
            ambient_with_follow_target.properties.push_back({ .name = "follow_target", .value = std::string{ "player" } });
            markers_layer.objects.push_back(std::move(ambient_with_follow_target));

            assets::tilemap_object_t second_ambient{ };
            second_ambient.name = "AmbientTwo";
            second_ambient.type = "Light";
            second_ambient.properties.push_back({ .name = "kind", .value = std::string{ "ambient" } });
            markers_layer.objects.push_back(std::move(second_ambient));

            assets::tilemap_object_t first_ambient{ };
            first_ambient.name = "AmbientOne";
            first_ambient.type = "Light";
            first_ambient.properties.push_back({ .name = "kind", .value = std::string{ "ambient" } });
            markers_layer.objects.push_back(std::move(first_ambient));

            tilemap.add_layer(std::move(markers_layer));

            const auto issues{ assets::validate_tiled_authored_data(tilemap) };
            const auto has_issue_code{ [&issues](const std::string_view code) {
                return std::ranges::any_of(issues, [code](const assets::tilemap_validation_issue_t& issue) {
                    return issue.code == code;
                });
            } };

            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.light.missing_kind"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.light.invalid_color"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.light.point.missing_radius"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.light.follow.missing_target"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.light.ambient.ignores_behavior"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.light.ambient.ignores_follow_target"));
            CARROT_TEST_REQUIRE(has_issue_code("tiled.object.light.multiple_ambient"));
        }

        void test_tiled_tile_animation_metadata_imports_from_sandbox_town()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.sandbox.town") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);

            const auto& tilesets{ tilemap->tilemap().tilesets() };
            const auto it{ std::ranges::find_if(tilesets, [](const assets::tilemap_tileset_t& tileset) {
                return tileset.name == "water";
            }) };
            CARROT_TEST_REQUIRE(it != tilesets.end());
            CARROT_TEST_REQUIRE(!it->tile_animations.empty());

            const assets::tilemap_tileset_t::tile_animation_t* water_animation{ it->find_tile_animation(95) };
            CARROT_TEST_REQUIRE(water_animation != nullptr);
            CARROT_TEST_REQUIRE(water_animation->frames.size() == 6u);
            CARROT_TEST_REQUIRE(water_animation->total_duration_ms == 600u);
            CARROT_TEST_REQUIRE(water_animation->frames.front().tile_id == 95u);
            CARROT_TEST_REQUIRE(water_animation->frames.back().tile_id == 100u);
        }

        void test_tile_animation_resolves_expected_frame_by_elapsed_time()
        {
            assets::tilemap_tileset_t tileset{ };
            tileset.tile_count = 16;
            tileset.tile_animations.push_back({
                .tile_id = 5,
                .frames = {
                    { .tile_id = 5, .duration_ms = 100 },
                    { .tile_id = 6, .duration_ms = 150 },
                    { .tile_id = 7, .duration_ms = 250 }
                }
            });
            tileset.rebuild_animation_lookup();

            CARROT_TEST_REQUIRE(tileset.resolve_animated_tile_id(5, 0) == 5u);
            CARROT_TEST_REQUIRE(tileset.resolve_animated_tile_id(5, 99) == 5u);
            CARROT_TEST_REQUIRE(tileset.resolve_animated_tile_id(5, 100) == 6u);
            CARROT_TEST_REQUIRE(tileset.resolve_animated_tile_id(5, 249) == 6u);
            CARROT_TEST_REQUIRE(tileset.resolve_animated_tile_id(5, 250) == 7u);
            CARROT_TEST_REQUIRE(tileset.resolve_animated_tile_id(5, 499) == 7u);
            CARROT_TEST_REQUIRE(tileset.resolve_animated_tile_id(5, 500) == 5u);
            CARROT_TEST_REQUIRE(tileset.resolve_animated_tile_id(4, 250) == 4u);
        }

        void test_tiled_tilemap_import_accepts_unsupported_features_non_fatally()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };

            const auto manifests{ assets::asset_discovery_t::discover_supported_manifests(vfs) };
            const auto it{ std::find(manifests.tilemaps.begin(), manifests.tilemaps.end(), "game://tilemaps/test_town.tilemap.json") };
            CARROT_TEST_REQUIRE(it != manifests.tilemaps.end());

            utils::json::json_document_t doc{ parse_json(vfs, *it) };
            CARROT_TEST_REQUIRE(assets::tilemap_asset_manifest_importer_t::import(doc, assets.tilemaps().registry(), vfs));
            CARROT_TEST_REQUIRE(assets.tilemaps().registry().find("tilemap.sandbox.town") != nullptr);
        }

        void test_scene_loader_loads_scene_successfully()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.test.overworld"));

            const world::world_object_t* player{ world.find_object_by_name("Vraden") };
            const world::world_object_t* spawn{ world.find_object_by_name("PlayerSpawn") };
            const world::world_object_t* map{ world.find_object_by_name("OverworldMap") };

            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(spawn != nullptr);
            CARROT_TEST_REQUIRE(map != nullptr);
            CARROT_TEST_REQUIRE(player->transform.has_value());
            CARROT_TEST_REQUIRE(spawn->transform.has_value());
            CARROT_TEST_REQUIRE(map->tilemap.has_value());
            CARROT_TEST_REQUIRE(world.find_object_by_name("WelcomeSign") != nullptr);
            CARROT_TEST_REQUIRE(player->transform->position.x == spawn->transform->position.x);
            CARROT_TEST_REQUIRE(player->transform->position.y == spawn->transform->position.y);
            CARROT_TEST_REQUIRE(world.presentation_pixels_per_unit() == world::world_units_t::default_pixels_per_unit);

            const assets::scene_asset_record_t* scene{ assets.scenes().registry().find("scene.test.overworld") };
            CARROT_TEST_REQUIRE(scene != nullptr);
            CARROT_TEST_REQUIRE(scene->scene.camera.zoom == 4.f);
            CARROT_TEST_REQUIRE(scene->scene.camera.follow_mode == assets::scene_camera_follow_mode_t::player);
            CARROT_TEST_REQUIRE(scene->scene.camera.initial_target_policy ==
                                assets::scene_camera_initial_target_policy_t::player);
            CARROT_TEST_REQUIRE(scene->scene.camera.dead_zone_size_world.x == 2.0f);
            CARROT_TEST_REQUIRE(scene->scene.camera.dead_zone_size_world.y == 1.5f);
            CARROT_TEST_REQUIRE(scene->scene.camera.follow_smoothing == 10.0f);
        }

        void test_scene_loader_loads_sandbox_town_successfully()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));

            const world::world_object_t* player{ world.find_object_by_name("Vraden") };
            const world::world_object_t* spawn{ world.find_object_by_name("PlayerSpawn") };
            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(spawn != nullptr);
            CARROT_TEST_REQUIRE(player->transform.has_value());
            CARROT_TEST_REQUIRE(spawn->transform.has_value());
            CARROT_TEST_REQUIRE(player->transform->position.x == spawn->transform->position.x);
            CARROT_TEST_REQUIRE(player->transform->position.y == spawn->transform->position.y);
            CARROT_TEST_REQUIRE(world.find_object_by_name("DoorToInn") != nullptr);
            CARROT_TEST_REQUIRE(world.find_object_by_name("DoorToItemShop") != nullptr);
            CARROT_TEST_REQUIRE(world.find_object_by_name("InnExteriorSpawn") != nullptr);
            CARROT_TEST_REQUIRE(world.find_object_by_name("ItemShopExteriorSpawn") != nullptr);

            const assets::scene_asset_record_t* scene{ assets.scenes().registry().find("scene.sandbox.town") };
            CARROT_TEST_REQUIRE(scene != nullptr);
            CARROT_TEST_REQUIRE(scene->scene.initial_music_id == "music.oak_battle_theme");
            CARROT_TEST_REQUIRE(scene->scene.camera.zoom == 2.f);
        }

        void test_scene_loader_fails_for_missing_scene()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };

            world::world_t world;
            CARROT_TEST_REQUIRE(!world::scene_loader_t::load_scene(world, assets, "scene.missing"));
        }

        void test_scene_loader_supports_spawn_override()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, static_cast<rhi::rhi_context_t&>(rhi) };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.test.overworld", "ExitNorth"));

            const world::world_object_t* player{ world.find_object_by_name("Vraden") };
            const world::world_object_t* marker{ world.find_object_by_name("ExitNorth") };
            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(marker != nullptr);
            CARROT_TEST_REQUIRE(player->transform.has_value());
            CARROT_TEST_REQUIRE(marker->transform.has_value());
            CARROT_TEST_REQUIRE(player->transform->position.x == marker->transform->position.x);
            CARROT_TEST_REQUIRE(player->transform->position.y == marker->transform->position.y);
        }

        void test_scene_loader_supports_sandbox_town_spawn_overrides()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, static_cast<rhi::rhi_context_t&>(rhi) };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town", "InnExteriorSpawn"));

            const world::world_object_t* player{ world.find_object_by_name("Vraden") };
            const world::world_object_t* marker{ world.find_object_by_name("InnExteriorSpawn") };
            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(marker != nullptr);
            CARROT_TEST_REQUIRE(player->transform.has_value());
            CARROT_TEST_REQUIRE(marker->transform.has_value());
            CARROT_TEST_REQUIRE(player->transform->position.x == marker->transform->position.x);
            CARROT_TEST_REQUIRE(player->transform->position.y == marker->transform->position.y);
        }

        void test_scene_loader_fails_for_missing_spawn_marker()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, static_cast<rhi::rhi_context_t&>(rhi) };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(!world::scene_loader_t::load_scene(world, assets, "scene.test.overworld", "MissingMarker"));
        }

        void test_scene_loader_preserves_existing_world_on_failed_spawn_override()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, static_cast<rhi::rhi_context_t&>(rhi) };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.test.overworld"));

            const size_t object_count_before_failure{ world.objects().size() };
            const world::world_object_t* player_before_failure{ world.find_object_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player_before_failure != nullptr);
            CARROT_TEST_REQUIRE(player_before_failure->transform.has_value());
            const chlm::float2 player_position_before_failure{ player_before_failure->transform->position };

            CARROT_TEST_REQUIRE(!world::scene_loader_t::load_scene(world, assets, "scene.test.overworld", "MissingMarker"));

            const world::world_object_t* player_after_failure{ world.find_object_by_name("Vraden") };
            const world::world_object_t* spawn_after_failure{ world.find_object_by_name("PlayerSpawn") };
            CARROT_TEST_REQUIRE(player_after_failure != nullptr);
            CARROT_TEST_REQUIRE(spawn_after_failure != nullptr);
            CARROT_TEST_REQUIRE(player_after_failure->transform.has_value());
            CARROT_TEST_REQUIRE(world.objects().size() == object_count_before_failure);
            CARROT_TEST_REQUIRE(player_after_failure->transform->position.x == player_position_before_failure.x);
            CARROT_TEST_REQUIRE(player_after_failure->transform->position.y == player_position_before_failure.y);
        }

        void test_scene_asset_importer_parses_camera_modes()
        {
            constexpr const char* manifest{
                R"({
                  "id": "scene.test.camera_modes",
                  "tilemap": "tilemap.test.overworld",
                  "player_sprite": "sprite.vraden",
                  "camera": {
                    "zoom": 2.5,
                    "follow_mode": "none",
                    "initial_target": "spawn_marker",
                    "dead_zone_world_size": { "x": 3.0, "y": 2.0 },
                    "follow_smoothing": 6.5
                  }
                })"
            };

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_memory(manifest, std::strlen(manifest)));

            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);
            assets::scene_asset_registry_t registry;
            CARROT_TEST_REQUIRE(assets::scene_asset_manifest_importer_t::import(doc, registry, vfs, "memory://scene.test.camera_modes"));

            const assets::scene_asset_record_t* scene{ registry.find("scene.test.camera_modes") };
            CARROT_TEST_REQUIRE(scene != nullptr);
            CARROT_TEST_REQUIRE(scene->scene.camera.zoom == 2.5f);
            CARROT_TEST_REQUIRE(scene->scene.camera.follow_mode == assets::scene_camera_follow_mode_t::none);
            CARROT_TEST_REQUIRE(scene->scene.camera.initial_target_policy ==
                                assets::scene_camera_initial_target_policy_t::spawn_marker);
            CARROT_TEST_REQUIRE(scene->scene.camera.dead_zone_size_world.x == 3.0f);
            CARROT_TEST_REQUIRE(scene->scene.camera.dead_zone_size_world.y == 2.0f);
            CARROT_TEST_REQUIRE(scene->scene.camera.follow_smoothing == 6.5f);
        }

        void test_scene_asset_importer_rejects_empty_spawn_marker()
        {
            constexpr const char* manifest{
                R"({
                  "id": "scene.test.invalid_spawn_marker",
                  "tilemap": "tilemap.test.overworld",
                  "player_sprite": "sprite.vraden",
                  "player_spawn_marker": ""
                })"
            };

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_memory(manifest, std::strlen(manifest)));

            io::virtual_file_system_t vfs;
            assets::scene_asset_registry_t registry;
            CARROT_TEST_REQUIRE(!assets::scene_asset_manifest_importer_t::import(doc, registry, vfs, "memory://scene.test.invalid_spawn_marker"));
        }

        void test_scene_asset_reference_validation_rejects_missing_tilemap()
        {
            constexpr const char* manifest{
                R"({
                  "id": "scene.test.missing_tilemap_ref",
                  "tilemap": "tilemap.missing",
                  "player_sprite": "sprite.vraden"
                })"
            };

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_memory(manifest, std::strlen(manifest)));

            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);
            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            CARROT_TEST_REQUIRE(assets::scene_asset_manifest_importer_t::import(doc, assets.scenes().registry(), vfs, "memory://scene.test.missing_tilemap_ref"));
            const assets::scene_asset_record_t* scene{ assets.scenes().registry().find("scene.test.missing_tilemap_ref") };
            CARROT_TEST_REQUIRE(scene != nullptr);
            CARROT_TEST_REQUIRE(!assets.scenes().registry().validate_references(*scene,
                                                                                assets.tilemaps().registry(),
                                                                                assets.sprites().registry(),
                                                                                assets.audio().registry()));
        }

        void test_scene_asset_reference_validation_rejects_missing_player_sprite()
        {
            constexpr const char* manifest{
                R"({
                  "id": "scene.test.missing_player_sprite_ref",
                  "tilemap": "tilemap.test.overworld",
                  "player_sprite": "sprite.missing"
                })"
            };

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_memory(manifest, std::strlen(manifest)));

            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);
            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            CARROT_TEST_REQUIRE(assets::scene_asset_manifest_importer_t::import(doc, assets.scenes().registry(), vfs, "memory://scene.test.missing_player_sprite_ref"));
            const assets::scene_asset_record_t* scene{ assets.scenes().registry().find("scene.test.missing_player_sprite_ref") };
            CARROT_TEST_REQUIRE(scene != nullptr);
            CARROT_TEST_REQUIRE(!assets.scenes().registry().validate_references(*scene,
                                                                                assets.tilemaps().registry(),
                                                                                assets.sprites().registry(),
                                                                                assets.audio().registry()));
        }

        void test_scene_asset_reference_validation_rejects_missing_initial_music()
        {
            constexpr const char* manifest{
                R"({
                  "id": "scene.test.missing_music_ref",
                  "tilemap": "tilemap.test.overworld",
                  "player_sprite": "sprite.vraden",
                  "initial_music": "music.missing"
                })"
            };

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_memory(manifest, std::strlen(manifest)));

            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);
            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            CARROT_TEST_REQUIRE(assets::scene_asset_manifest_importer_t::import(doc, assets.scenes().registry(), vfs, "memory://scene.test.missing_music_ref"));
            const assets::scene_asset_record_t* scene{ assets.scenes().registry().find("scene.test.missing_music_ref") };
            CARROT_TEST_REQUIRE(scene != nullptr);
            CARROT_TEST_REQUIRE(!assets.scenes().registry().validate_references(*scene,
                                                                                assets.tilemaps().registry(),
                                                                                assets.sprites().registry(),
                                                                                assets.audio().registry()));
        }

        void test_door_transition_request_rejects_unknown_target_scene()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, static_cast<rhi::rhi_context_t&>(rhi) };
            register_required_assets(assets, vfs);

            world::world_object_t door;
            door.name = "BadSceneDoor";
            door.type = "Door";
            door.properties = {
                assets::tilemap_property_t{ .name = "target_scene", .value = std::string{ "scene.missing" } },
                assets::tilemap_property_t{ .name = "target_marker", .value = std::string{ "Entry" } }
            };

            CARROT_TEST_REQUIRE(!carrot::world::authored::make_scene_transition_request(assets, door).has_value());
            CARROT_TEST_REQUIRE(!carrot::world::authored::validate_scene_transition_target(assets, door));
        }

        void test_door_transition_request_rejects_unresolved_legacy_target_map()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, static_cast<rhi::rhi_context_t&>(rhi) };
            register_required_assets(assets, vfs);

            world::world_object_t door;
            door.name = "BadLegacyDoor";
            door.type = "Door";
            door.properties = {
                assets::tilemap_property_t{ .name = "target_map", .value = std::string{ "tilemap.missing" } },
                assets::tilemap_property_t{ .name = "target_marker", .value = std::string{ "Entry" } }
            };

            CARROT_TEST_REQUIRE(!carrot::world::authored::make_scene_transition_request(assets, door).has_value());
            CARROT_TEST_REQUIRE(!carrot::world::authored::validate_scene_transition_target(assets, door));
        }

        void test_door_transition_request_rejects_missing_target_marker()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, static_cast<rhi::rhi_context_t&>(rhi) };
            register_required_assets(assets, vfs);

            world::world_object_t door;
            door.name = "MissingMarkerDoor";
            door.type = "Door";
            door.properties = {
                assets::tilemap_property_t{ .name = "target_scene", .value = std::string{ "scene.test.overworld" } }
            };

            CARROT_TEST_REQUIRE(!carrot::world::authored::make_scene_transition_request(assets, door).has_value());
            CARROT_TEST_REQUIRE(!carrot::world::authored::validate_scene_transition_target(assets, door));
        }

        void test_validate_scene_transition_targets_rejects_invalid_door_in_world()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, static_cast<rhi::rhi_context_t&>(rhi) };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.test.overworld"));

            world::world_object_t& invalid_door{ world.create_object() };
            invalid_door.name = "InjectedBadDoor";
            invalid_door.type = "Door";
            invalid_door.properties = {
                assets::tilemap_property_t{ .name = "target_scene", .value = std::string{ "scene.missing" } },
                assets::tilemap_property_t{ .name = "target_marker", .value = std::string{ "Entry" } }
            };

            CARROT_TEST_REQUIRE(!carrot::world::authored::validate_scene_transition_targets(assets, world));
        }

        void test_validate_scene_transition_targets_rejects_missing_destination_marker()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, static_cast<rhi::rhi_context_t&>(rhi) };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));

            world::world_object_t& invalid_door{ world.create_object() };
            invalid_door.name = "BadDestinationMarkerDoor";
            invalid_door.type = "Door";
            invalid_door.properties = {
                assets::tilemap_property_t{ .name = "target_scene", .value = std::string{ "scene.sandbox.inn" } },
                assets::tilemap_property_t{ .name = "target_marker", .value = std::string{ "MissingMarker" } }
            };

            CARROT_TEST_REQUIRE(!carrot::world::authored::validate_scene_transition_targets(assets, world));
        }

        void test_scene_validation_report_does_not_realize_destination_tilemaps()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, static_cast<rhi::rhi_context_t&>(rhi) };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));

            CARROT_TEST_REQUIRE(!assets.tilemaps().is_loaded("tilemap.sandbox.inn"));
            CARROT_TEST_REQUIRE(!assets.tilemaps().is_loaded("tilemap.sandbox.item_shop"));

            const carrot::world::authored::scene_validation_report_t report{
                carrot::world::authored::build_scene_validation_report(assets, world)
            };

            CARROT_TEST_REQUIRE(report.valid());
            CARROT_TEST_REQUIRE(!assets.tilemaps().is_loaded("tilemap.sandbox.inn"));
            CARROT_TEST_REQUIRE(!assets.tilemaps().is_loaded("tilemap.sandbox.item_shop"));
        }

        void test_sandbox_scene_transition_requests_connect_all_three_scenes()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, static_cast<rhi::rhi_context_t&>(rhi) };
            register_required_assets(assets, vfs);

            world::world_t town_world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(town_world, assets, "scene.sandbox.town"));
            const world::world_object_t* town_to_inn{ town_world.find_object_by_name("DoorToInn") };
            const world::world_object_t* town_to_item_shop{ town_world.find_object_by_name("DoorToItemShop") };
            CARROT_TEST_REQUIRE(town_to_inn != nullptr);
            CARROT_TEST_REQUIRE(town_to_item_shop != nullptr);

            const std::optional<carrot::scene::scene_transition_request_t> inn_request{
                carrot::world::authored::make_scene_transition_request(assets, *town_to_inn)
            };
            const std::optional<carrot::scene::scene_transition_request_t> item_shop_request{
                carrot::world::authored::make_scene_transition_request(assets, *town_to_item_shop)
            };
            CARROT_TEST_REQUIRE(inn_request.has_value());
            CARROT_TEST_REQUIRE(item_shop_request.has_value());
            CARROT_TEST_REQUIRE(inn_request->scene_id == "scene.sandbox.inn");
            CARROT_TEST_REQUIRE(inn_request->marker_name == "EntryFromTown");
            CARROT_TEST_REQUIRE(item_shop_request->scene_id == "scene.sandbox.item_shop");
            CARROT_TEST_REQUIRE(item_shop_request->marker_name == "EntryFromTown");

            world::world_t inn_world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(inn_world, assets, inn_request->scene_id, inn_request->marker_name));
            const world::world_object_t* inn_exit_door{ inn_world.find_object_by_name("DoorToTownFromInn") };
            CARROT_TEST_REQUIRE(inn_exit_door != nullptr);
            const std::optional<carrot::scene::scene_transition_request_t> inn_exit_request{
                carrot::world::authored::make_scene_transition_request(assets, *inn_exit_door)
            };
            CARROT_TEST_REQUIRE(inn_exit_request.has_value());
            CARROT_TEST_REQUIRE(inn_exit_request->scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(inn_exit_request->marker_name == "InnExteriorSpawn");

            world::world_t item_shop_world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(item_shop_world, assets, item_shop_request->scene_id, item_shop_request->marker_name));
            const world::world_object_t* item_shop_exit_door{ item_shop_world.find_object_by_name("DoorToTownFromItemShop") };
            CARROT_TEST_REQUIRE(item_shop_exit_door != nullptr);
            const std::optional<carrot::scene::scene_transition_request_t> item_shop_exit_request{
                carrot::world::authored::make_scene_transition_request(assets, *item_shop_exit_door)
            };
            CARROT_TEST_REQUIRE(item_shop_exit_request.has_value());
            CARROT_TEST_REQUIRE(item_shop_exit_request->scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(item_shop_exit_request->marker_name == "ItemShopExteriorSpawn");
        }

        void test_transition_runtime_state_preserves_opened_container_across_scene_reload()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, static_cast<rhi::rhi_context_t&>(rhi) };
            register_required_assets(assets, vfs);

            sandbox::gameplay_runtime_state_t runtime_state;
            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));

            const world::world_object_t* first_load_chest{ world.find_object_by_name("StarterChest") };
            CARROT_TEST_REQUIRE(first_load_chest != nullptr);
            CARROT_TEST_REQUIRE(first_load_chest->get_bool_property("interactable").value_or(false));

            sandbox::mark_container_open(runtime_state, "scene.sandbox.town", *first_load_chest);
            sandbox::apply_runtime_state_to_scene("scene.sandbox.town", world, runtime_state);

            const world::world_object_t* opened_chest{ world.find_object_by_name("StarterChest") };
            CARROT_TEST_REQUIRE(opened_chest != nullptr);
            CARROT_TEST_REQUIRE(!opened_chest->get_bool_property("interactable").value_or(true));

            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));
            sandbox::apply_runtime_state_to_scene("scene.sandbox.town", world, runtime_state);

            const world::world_object_t* reloaded_chest{ world.find_object_by_name("StarterChest") };
            CARROT_TEST_REQUIRE(reloaded_chest != nullptr);
            CARROT_TEST_REQUIRE(sandbox::is_container_open(runtime_state, "scene.sandbox.town", *reloaded_chest));
            CARROT_TEST_REQUIRE(!reloaded_chest->get_bool_property("interactable").value_or(true));
        }

        void test_transition_runtime_state_restores_player_facing_after_rebind()
        {
            sandbox::gameplay_runtime_state_t runtime_state;
            carrot::world::player_controller_t controller;

            carrot::world::world_object_t original_player;
            original_player.transform = carrot::world::transform_component_t{
                .position = { 0.f, 0.f },
                .scale = { 1.f, 1.f }
            };
            controller.set_controlled_object(&original_player);
            controller.set_facing_direction(carrot::world::facing_direction_t::left);

            sandbox::capture_player_runtime_state(runtime_state, controller);
            CARROT_TEST_REQUIRE(runtime_state.player_facing.has_value());
            CARROT_TEST_REQUIRE(*runtime_state.player_facing == carrot::world::facing_direction_t::left);

            carrot::world::world_object_t transitioned_player;
            transitioned_player.transform = carrot::world::transform_component_t{
                .position = { 5.f, 3.f },
                .scale = { 1.f, 1.f }
            };
            controller.set_controlled_object(&transitioned_player);
            CARROT_TEST_REQUIRE(controller.facing_direction() == carrot::world::facing_direction_t::down);

            sandbox::apply_runtime_state_to_player(runtime_state, controller);
            CARROT_TEST_REQUIRE(controller.facing_direction() == carrot::world::facing_direction_t::left);
        }

        void test_scene_continuity_builds_stable_object_keys_from_name_and_source()
        {
            carrot::world::world_object_t named_object;
            named_object.id = 10;
            named_object.name = "StarterChest";

            CARROT_TEST_REQUIRE(carrot::world::build_runtime_object_identity(named_object) == "name:StarterChest");
            CARROT_TEST_REQUIRE(carrot::world::make_scene_runtime_object_key("scene.sandbox.town", named_object) ==
                                "scene.sandbox.town::name:StarterChest");

            carrot::world::world_object_t sourced_object;
            sourced_object.id = 99;
            sourced_object.source = carrot::world::world_object_source_t{
                .tilemap_logical_id = "tilemap.sandbox.town",
                .layer_name = "Objects",
                .object_id = 42,
                .object_name = "UnnamedSource"
            };

            CARROT_TEST_REQUIRE(carrot::world::build_runtime_object_identity(sourced_object) ==
                                "source:tilemap.sandbox.town:Objects:42");
            CARROT_TEST_REQUIRE(carrot::world::make_scene_runtime_object_flag_key("scene.sandbox.town",
                                                                                  sourced_object,
                                                                                  "opened") ==
                                "scene.sandbox.town::source:tilemap.sandbox.town:Objects:42::opened");
        }

        void test_scene_continuity_flag_store_tracks_named_flags_per_scene_object()
        {
            carrot::world::scene_runtime_flag_store_t flags;
            carrot::world::world_object_t object;
            object.name = "StarterChest";

            CARROT_TEST_REQUIRE(!flags.contains("scene.sandbox.town", object, "opened"));
            flags.mark("scene.sandbox.town", object, "opened");
            CARROT_TEST_REQUIRE(flags.contains("scene.sandbox.town", object, "opened"));
            CARROT_TEST_REQUIRE(!flags.contains("scene.sandbox.inn", object, "opened"));
            CARROT_TEST_REQUIRE(!flags.contains("scene.sandbox.town", object, "looted"));
            CARROT_TEST_REQUIRE(flags.size() == 1u);
        }

        void test_scene_continuity_applies_flagged_callback_to_matching_objects()
        {
            carrot::world::scene_runtime_flag_store_t flags;
            carrot::world::world_t world;

            carrot::world::world_object_t& chest{ world.create_object() };
            chest.name = "StarterChest";
            chest.type = "Container";
            chest.properties.emplace_back(carrot::assets::tilemap_property_t{
                .name = "interactable",
                .value = true
            });

            carrot::world::world_object_t& sign{ world.create_object() };
            sign.name = "WelcomeSign";
            sign.type = "Sign";
            sign.properties.emplace_back(carrot::assets::tilemap_property_t{
                .name = "interactable",
                .value = true
            });

            const carrot::world::world_object_t* opened_chest{ world.find_object_by_name("StarterChest") };
            CARROT_TEST_REQUIRE(opened_chest != nullptr);
            flags.mark("scene.sandbox.town", *opened_chest, "opened");

            const size_t applied{
                carrot::world::apply_scene_runtime_flag_to_matching_objects(
                    "scene.sandbox.town",
                    world,
                    flags,
                    "opened",
                    [](const carrot::world::world_object_t& object)
                    {
                        return object.type == "Container";
                    },
                    [](carrot::world::world_object_t& object)
                    {
                        carrot::world::set_world_object_bool_property(object, "interactable", false);
                    }
                )
            };

            const carrot::world::world_object_t* applied_chest{ world.find_object_by_name("StarterChest") };
            const carrot::world::world_object_t* applied_sign{ world.find_object_by_name("WelcomeSign") };

            CARROT_TEST_REQUIRE(applied == 1u);
            CARROT_TEST_REQUIRE(applied_chest != nullptr);
            CARROT_TEST_REQUIRE(applied_sign != nullptr);
            CARROT_TEST_REQUIRE(!applied_chest->get_bool_property("interactable").value_or(true));
            CARROT_TEST_REQUIRE(applied_sign->get_bool_property("interactable").value_or(false));
        }

        void test_scene_runtime_snapshot_defaults_to_idle_state()
        {
            carrot::scene::scene_runtime_t runtime;
            const carrot::scene::scene_runtime_snapshot_t snapshot{ runtime.snapshot() };

            CARROT_TEST_REQUIRE(snapshot.runtime_state == carrot::scene::scene_runtime_state_t::idle);
            CARROT_TEST_REQUIRE(snapshot.transition_phase == carrot::scene::scene_transition_phase_t::none);
            CARROT_TEST_REQUIRE(snapshot.pending_request_kind == carrot::scene::scene_change_request_kind_t::none);
            CARROT_TEST_REQUIRE(!snapshot.has_active_scene());
            CARROT_TEST_REQUIRE(!snapshot.has_pending_scene());
            CARROT_TEST_REQUIRE(!snapshot.is_transitioning());
            CARROT_TEST_REQUIRE(runtime.current_scene_id().empty());
            CARROT_TEST_REQUIRE(runtime.current_spawn_marker().empty());
            CARROT_TEST_REQUIRE(runtime.pending_scene_id().empty());
            CARROT_TEST_REQUIRE(runtime.pending_spawn_marker().empty());
            CARROT_TEST_REQUIRE(!runtime.has_scene_loaded());
            CARROT_TEST_REQUIRE(!runtime.has_pending_scene());
            CARROT_TEST_REQUIRE(!runtime.is_transitioning());
            CARROT_TEST_REQUIRE(runtime.runtime_state() == carrot::scene::scene_runtime_state_t::idle);
            CARROT_TEST_REQUIRE(runtime.transition_phase() == carrot::scene::scene_transition_phase_t::none);
        }

        void test_scene_runtime_state_labels_match_expected_diagnostics()
        {
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_runtime_state_t::idle) == "idle");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_runtime_state_t::loading) == "loading");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_runtime_state_t::active) == "active");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_runtime_state_t::transitioning) == "transitioning");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_phase_t::none) == "none");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_phase_t::preparing) == "preparing");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_phase_t::loading) == "loading");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_phase_t::activating) == "activating");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_phase_t::finalizing) == "finalizing");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_change_request_kind_t::none) == "none");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_change_request_kind_t::load) == "load");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_change_request_kind_t::transition) == "transition");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_change_request_kind_t::rebuild) == "rebuild");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_change_outcome_t::none) == "none");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_change_outcome_t::in_progress) == "in_progress");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_change_outcome_t::succeeded) == "succeeded");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_change_outcome_t::failed) == "failed");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_overlay_style_t::inherit) == "inherit");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_overlay_style_t::none) == "none");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_overlay_style_t::fade) == "fade");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_overlay_style_t::loading_screen) == "loading_screen");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_overlay_style_t::wipe) == "wipe");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_effect_t::inherit) == "inherit");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_effect_t::none) == "none");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_effect_t::fade) == "fade");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_effect_t::loading_screen) == "loading_screen");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_effect_t::wipe) == "wipe");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_effect_t::battle_swirl) == "battle_swirl");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_wipe_direction_t::left_to_right) == "left_to_right");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_wipe_direction_t::right_to_left) == "right_to_left");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_wipe_direction_t::top_to_bottom) == "top_to_bottom");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_transition_wipe_direction_t::bottom_to_top) == "bottom_to_top");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_camera_projection_mode_t::orthographic) == "orthographic");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_camera_projection_mode_t::perspective) == "perspective");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_camera_bounds_mode_t::none) == "none");
            CARROT_TEST_REQUIRE(carrot::scene::to_string(carrot::scene::scene_camera_bounds_mode_t::scene_extents) == "scene_extents");
        }

        void test_scene_runtime_summary_defaults_to_empty_world_state()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            const scene::scene_runtime_summary_t summary{ runtime.summarize(game) };

            CARROT_TEST_REQUIRE(summary.snapshot.runtime_state == carrot::scene::scene_runtime_state_t::idle);
            CARROT_TEST_REQUIRE(summary.snapshot.transition_phase == carrot::scene::scene_transition_phase_t::none);
            CARROT_TEST_REQUIRE(summary.snapshot.active_scene_id.empty());
            CARROT_TEST_REQUIRE(!summary.diagnostics.visible);
            CARROT_TEST_REQUIRE(summary.diagnostics.request_kind == carrot::scene::scene_change_request_kind_t::none);
            CARROT_TEST_REQUIRE(summary.diagnostics.outcome == carrot::scene::scene_change_outcome_t::none);
            CARROT_TEST_REQUIRE(summary.diagnostics.overlay_opacity == 0.f);
            CARROT_TEST_REQUIRE(!summary.diagnostics.show_loading_text);
            CARROT_TEST_REQUIRE(summary.world_object_count == 0u);
            CARROT_TEST_REQUIRE(summary.trigger_count == 0u);
            CARROT_TEST_REQUIRE(summary.object_collider_count == 0u);
            CARROT_TEST_REQUIRE(summary.static_collider_count == 0u);
            CARROT_TEST_REQUIRE(summary.point_light_count == 0u);
            CARROT_TEST_REQUIRE(summary.visibility_region_count == 0u);
            CARROT_TEST_REQUIRE(!summary.has_player_object());
            CARROT_TEST_REQUIRE(!summary.has_spawn_object());
        }

        void test_scene_runtime_summary_reports_loaded_scene_world_state()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            CARROT_TEST_REQUIRE(runtime.load(game,
                                             "scene.sandbox.town",
                                             carrot::scene::scene_load_options_t{
                                                 .apply_scene_music = false,
                                                 .transition_overlay = {
                                                     .style = carrot::scene::scene_transition_overlay_style_t::none
                                                 }
                                             }));

            const scene::scene_runtime_summary_t summary{ runtime.summarize(game) };

            CARROT_TEST_REQUIRE(summary.snapshot.active_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(summary.snapshot.active_spawn_marker == "PlayerSpawn");
            CARROT_TEST_REQUIRE(summary.snapshot.runtime_state == carrot::scene::scene_runtime_state_t::active);
            CARROT_TEST_REQUIRE(summary.snapshot.transition_phase == carrot::scene::scene_transition_phase_t::none);
            CARROT_TEST_REQUIRE(summary.snapshot.transition_progress == 0.f);
            CARROT_TEST_REQUIRE(summary.diagnostics.visible);
            CARROT_TEST_REQUIRE(summary.diagnostics.request_kind == carrot::scene::scene_change_request_kind_t::load);
            CARROT_TEST_REQUIRE(summary.diagnostics.outcome == carrot::scene::scene_change_outcome_t::succeeded);
            CARROT_TEST_REQUIRE(summary.diagnostics.transition_effect == carrot::scene::scene_transition_effect_t::fade);
            CARROT_TEST_REQUIRE(summary.diagnostics.overlay_style == carrot::scene::scene_transition_overlay_style_t::fade);
            CARROT_TEST_REQUIRE(summary.diagnostics.overlay_opacity == 0.f);
            CARROT_TEST_REQUIRE(!summary.diagnostics.show_loading_text);
            CARROT_TEST_REQUIRE(summary.diagnostics.target_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(summary.diagnostics.target_spawn_marker == "PlayerSpawn");
            CARROT_TEST_REQUIRE(!summary.diagnostics.preserved_active_scene);
            CARROT_TEST_REQUIRE(summary.world_object_count > 0u);
            CARROT_TEST_REQUIRE(summary.trigger_count > 0u);
            CARROT_TEST_REQUIRE(summary.object_collider_count > 0u);
            CARROT_TEST_REQUIRE(summary.static_collider_count > 0u);
            CARROT_TEST_REQUIRE(summary.has_player_object());
            CARROT_TEST_REQUIRE(!summary.player_object_name.empty());
            CARROT_TEST_REQUIRE(summary.has_spawn_object());
            CARROT_TEST_REQUIRE(summary.spawn_object_name == "PlayerSpawn");
            CARROT_TEST_REQUIRE(summary.active_camera.zoom > 0.f);

            const chlm::float2 camera_center{ summary.camera_center_world };
            CARROT_TEST_REQUIRE(camera_center.x > 0.f);
            CARROT_TEST_REQUIRE(camera_center.y > 0.f);
        }

        void test_scene_runtime_object_summaries_report_loaded_scene_objects()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            CARROT_TEST_REQUIRE(runtime.load(game,
                                             "scene.sandbox.town",
                                             carrot::scene::scene_load_options_t{
                                                 .apply_scene_music = false,
                                                 .transition_overlay = {
                                                     .style = carrot::scene::scene_transition_overlay_style_t::none
                                                 }
                                             }));

            const std::vector<scene::scene_runtime_object_summary_t> summaries{
                runtime.collect_runtime_object_summaries(game)
            };

            CARROT_TEST_REQUIRE(!summaries.empty());
            CARROT_TEST_REQUIRE(summaries.size() == world.objects().size());

            const auto find_summary_by_name = [&summaries](const std::string_view name)
                -> const scene::scene_runtime_object_summary_t*
            {
                for (const scene::scene_runtime_object_summary_t& summary : summaries)
                {
                    if (summary.name == name)
                        return &summary;
                }

                return nullptr;
            };

            const scene::scene_runtime_object_summary_t* player_summary{ find_summary_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player_summary != nullptr);
            CARROT_TEST_REQUIRE(player_summary->id != 0u);
            CARROT_TEST_REQUIRE(!player_summary->type.empty());
            CARROT_TEST_REQUIRE(player_summary->has_transform);
            CARROT_TEST_REQUIRE(player_summary->has_collision);
            CARROT_TEST_REQUIRE(player_summary->has_sprite);
            CARROT_TEST_REQUIRE(player_summary->has_sprite_animator);
            CARROT_TEST_REQUIRE(player_summary->sprite.texture_id == "texture.vraden_sprite");
            CARROT_TEST_REQUIRE(!player_summary->sprite_animator.current_animation_name.empty());

            const scene::scene_runtime_object_summary_t* map_summary{ nullptr };
            for (const scene::scene_runtime_object_summary_t& summary : summaries)
            {
                if (!summary.has_tilemap)
                    continue;

                if (summary.tilemap.tilemap_logical_id == "tilemap.sandbox.town")
                {
                    map_summary = &summary;
                    break;
                }
            }
            CARROT_TEST_REQUIRE(map_summary != nullptr);
            CARROT_TEST_REQUIRE(map_summary->has_tilemap);
            CARROT_TEST_REQUIRE(map_summary->tilemap.tilemap_logical_id == "tilemap.sandbox.town");

            const scene::scene_runtime_object_summary_t* trigger_summary{ find_summary_by_name("Trigger") };
            CARROT_TEST_REQUIRE(trigger_summary != nullptr);
            CARROT_TEST_REQUIRE(trigger_summary->has_trigger);
            CARROT_TEST_REQUIRE(trigger_summary->has_interaction);
            CARROT_TEST_REQUIRE(trigger_summary->interaction.kind ==
                                scene::scene_runtime_object_interaction_kind_t::trigger);
            CARROT_TEST_REQUIRE(trigger_summary->interaction.trigger_id == "inn_trigger_1");
            CARROT_TEST_REQUIRE(trigger_summary->has_source);
            CARROT_TEST_REQUIRE(trigger_summary->source.tilemap_logical_id == "tilemap.sandbox.town");

            const auto inspected_player{ runtime.find_runtime_object_summary(game, player_summary->id) };
            CARROT_TEST_REQUIRE(inspected_player.has_value());
            CARROT_TEST_REQUIRE(inspected_player->name == "Vraden");
            CARROT_TEST_REQUIRE(inspected_player->has_sprite);
            CARROT_TEST_REQUIRE(inspected_player->sprite.texture_id == "texture.vraden_sprite");

            const auto missing_object{ runtime.find_runtime_object_summary(game, 999999999u) };
            CARROT_TEST_REQUIRE(!missing_object.has_value());
            CARROT_TEST_REQUIRE(carrot::scene::to_string(scene::scene_runtime_object_interaction_kind_t::trigger) ==
                                "trigger");
        }

        void test_scene_runtime_systems_summary_defaults_to_unbound_state()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            const scene::scene_runtime_systems_summary_t summary{ runtime.summarize_runtime_systems(game) };

            CARROT_TEST_REQUIRE(summary.lighting.point_light_count == 0u);
            CARROT_TEST_REQUIRE(summary.lighting.point_lights.empty());
            CARROT_TEST_REQUIRE(!summary.post_fx.bloom_enabled);
            CARROT_TEST_REQUIRE(summary.post_fx.bloom_baseline_strength == 0.f);
            CARROT_TEST_REQUIRE(summary.post_fx.bloom_max_strength > 0.f);
            CARROT_TEST_REQUIRE(summary.post_fx.battle_swirl_supported);
            CARROT_TEST_REQUIRE(summary.post_fx.capture_based_transition_effects_available);
            CARROT_TEST_REQUIRE(summary.light_shafts.renderer_contract_ready);
            CARROT_TEST_REQUIRE(summary.light_shafts.composite_capture_source_available);
            CARROT_TEST_REQUIRE(summary.light_shafts.fullscreen_pass_orchestration_available);
            CARROT_TEST_REQUIRE(summary.light_shafts.point_light_source_input_available);
            CARROT_TEST_REQUIRE(summary.light_shafts.requires_world_occlusion_mask);
            CARROT_TEST_REQUIRE(summary.light_shafts.requires_source_mask_texture);
            CARROT_TEST_REQUIRE(summary.light_shafts.requires_authored_shaft_source_selection);
            CARROT_TEST_REQUIRE(summary.light_shafts.available_point_light_source_count == 0u);
            CARROT_TEST_REQUIRE(summary.collision.static_collider_count == 0u);
            CARROT_TEST_REQUIRE(!summary.collision.show_map_collision);
            CARROT_TEST_REQUIRE(!summary.collision.show_object_colliders);
            CARROT_TEST_REQUIRE(!summary.collision.show_trigger_volumes);
            CARROT_TEST_REQUIRE(!summary.layering.show_visibility_regions);
            CARROT_TEST_REQUIRE(summary.layering.active_visibility_tags.empty());
            CARROT_TEST_REQUIRE(!summary.player_controller.bound);
            CARROT_TEST_REQUIRE(!summary.player_controller.has_controlled_object());
            CARROT_TEST_REQUIRE(!summary.interaction_controller.bound);
            CARROT_TEST_REQUIRE(!summary.interaction_controller.has_actor());
            CARROT_TEST_REQUIRE(!summary.interaction_controller.has_candidate);
        }

        void test_scene_runtime_systems_summary_reports_bound_runtime_state()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            world::player_controller_t player_controller;
            world::interaction_controller_t interaction_controller;
            scene::scene_runtime_t runtime;
            CARROT_TEST_REQUIRE(runtime.load(game,
                                             "scene.sandbox.town",
                                             carrot::scene::scene_load_options_t{
                                                 .player_controller = &player_controller,
                                                 .interaction_controller = &interaction_controller,
                                                 .apply_scene_music = false,
                                                 .transition_overlay = {
                                                     .style = carrot::scene::scene_transition_overlay_style_t::none
                                                 }
                                             }));

            world.lighting() = world::world_lighting_state_t{ };
            world.lighting().ambient_color = { 0.2f, 0.3f, 0.4f, 1.f };
            world.lighting().point_lights.push_back(world::world_lighting_state_t::point_light_t{
                .position_world = { 12.f, 8.f },
                .radius_world = 6.f,
                .color = { 1.f, 0.8f, 0.4f, 1.f },
                .intensity = 1.5f
            });
            renderer.set_bloom_settings(renderer::bloom_settings_t{
                .enabled = true,
                .baseline_strength = 0.18f,
                .peak_light_response = 0.12f,
                .accumulated_light_response = 0.08f,
                .ambient_response = 0.06f,
                .max_strength = 0.3f,
                .tint_abgr = 0xFFE8F6FFu
            });
            world.collision_debug_view().show_map_collision = true;
            world.collision_debug_view().show_object_colliders = true;
            world.collision_debug_view().show_trigger_volumes = true;
            world.layering_debug_view().show_visibility_regions = true;
            world.layering_debug_view().visibility_region_color = 0x6633AAFFu;
            world::layering_debug_snapshot_t snapshot;
            snapshot.frame_index = 42u;
            snapshot.has_visibility_anchor = true;
            snapshot.visibility_anchor_world = { 5.f, 7.f };
            snapshot.visibility_region_count = 2u;
            snapshot.rendered_tilemap_count = 1u;
            snapshot.layer_count = 4u;
            snapshot.visible_layer_count = 3u;
            snapshot.hidden_layer_count = 1u;
            snapshot.visibility_bound_layer_count = 2u;
            snapshot.conditional_front_layer_count = 1u;
            snapshot.always_front_layer_count = 1u;
            snapshot.active_visibility_tags = { "inn_roof", "item_shop_roof" };
            world.set_layering_debug_snapshot(std::move(snapshot));

            world::world_object_t& interactable{ world.create_object() };
            interactable.name = "DebugSign";
            interactable.type = "Sign";
            const world::world_object_t* controlled_object{ player_controller.controlled_object() };
            CARROT_TEST_REQUIRE(controlled_object != nullptr);
            CARROT_TEST_REQUIRE(controlled_object->transform.has_value());
            const chlm::float2 actor_position{ controlled_object->transform->position };
            interactable.transform = world::transform_component_t{
                .position = { actor_position.x + 0.5f, actor_position.y }
            };
            interactable.properties.push_back({
                .name = "interactable",
                .value = true
            });
            interactable.properties.push_back({
                .name = "message_id",
                .value = std::string{ "debug.sign" }
            });

            const scene::scene_runtime_systems_summary_t summary{ runtime.summarize_runtime_systems(game) };

            CARROT_TEST_REQUIRE(summary.lighting.point_light_count == 1u);
            CARROT_TEST_REQUIRE(summary.lighting.point_lights.size() == 1u);
            CARROT_TEST_REQUIRE(summary.lighting.ambient_color.x == 0.2f);
            CARROT_TEST_REQUIRE(summary.lighting.point_lights.front().position_world.x == 12.f);
            CARROT_TEST_REQUIRE(summary.post_fx.bloom_enabled);
            CARROT_TEST_REQUIRE(summary.post_fx.bloom_baseline_strength == 0.18f);
            CARROT_TEST_REQUIRE(summary.post_fx.bloom_max_strength == 0.3f);
            CARROT_TEST_REQUIRE(summary.post_fx.bloom_tint_abgr == 0xFFE8F6FFu);
            CARROT_TEST_REQUIRE(summary.post_fx.battle_swirl_supported);
            CARROT_TEST_REQUIRE(summary.post_fx.capture_based_transition_effects_available);
            CARROT_TEST_REQUIRE(summary.light_shafts.renderer_contract_ready);
            CARROT_TEST_REQUIRE(summary.light_shafts.composite_capture_source_available);
            CARROT_TEST_REQUIRE(summary.light_shafts.fullscreen_pass_orchestration_available);
            CARROT_TEST_REQUIRE(summary.light_shafts.point_light_source_input_available);
            CARROT_TEST_REQUIRE(summary.light_shafts.requires_world_occlusion_mask);
            CARROT_TEST_REQUIRE(summary.light_shafts.requires_source_mask_texture);
            CARROT_TEST_REQUIRE(summary.light_shafts.requires_authored_shaft_source_selection);
            CARROT_TEST_REQUIRE(summary.light_shafts.available_point_light_source_count == 1u);
            CARROT_TEST_REQUIRE(summary.collision.static_collider_count > 0u);
            CARROT_TEST_REQUIRE(summary.collision.show_map_collision);
            CARROT_TEST_REQUIRE(summary.collision.show_object_colliders);
            CARROT_TEST_REQUIRE(summary.collision.show_trigger_volumes);
            CARROT_TEST_REQUIRE(summary.layering.show_visibility_regions);
            CARROT_TEST_REQUIRE(summary.layering.frame_index == 42u);
            CARROT_TEST_REQUIRE(summary.layering.active_visibility_tags.size() == 2u);
            CARROT_TEST_REQUIRE(summary.player_controller.bound);
            CARROT_TEST_REQUIRE(summary.player_controller.has_controlled_object());
            CARROT_TEST_REQUIRE(summary.player_controller.controlled_object_name == "Vraden");
            CARROT_TEST_REQUIRE(!summary.player_controller.facing_direction.empty());
            CARROT_TEST_REQUIRE(summary.player_controller.move_speed > 0.f);
            CARROT_TEST_REQUIRE(summary.interaction_controller.bound);
            CARROT_TEST_REQUIRE(summary.interaction_controller.has_actor());
            CARROT_TEST_REQUIRE(summary.interaction_controller.actor_object_name == "Vraden");
            CARROT_TEST_REQUIRE(summary.interaction_controller.has_candidate);
            CARROT_TEST_REQUIRE(summary.interaction_controller.candidate_object_name == "DebugSign");
            CARROT_TEST_REQUIRE(summary.interaction_controller.candidate_distance.has_value());
        }

        void test_transition_presentation_is_hidden_when_runtime_is_idle()
        {
            const carrot::scene::scene_transition_presentation_t presentation{
                carrot::scene::make_transition_presentation(carrot::scene::scene_runtime_snapshot_t{})
            };

            CARROT_TEST_REQUIRE(!presentation.visible);
            CARROT_TEST_REQUIRE(!presentation.show_loading_text);
            CARROT_TEST_REQUIRE(presentation.overlay_opacity == 0.f);
        }

        void test_transition_presentation_exposes_loading_overlay_during_prepare()
        {
            const carrot::scene::scene_runtime_snapshot_t snapshot{
                .runtime_state = carrot::scene::scene_runtime_state_t::loading,
                .transition_phase = carrot::scene::scene_transition_phase_t::preparing,
                .pending_scene_id = "scene.sandbox.town",
                .transition_completed_steps = 2u,
                .transition_total_steps = 8u,
                .transition_progress = 0.25f
            };

            const carrot::scene::scene_transition_presentation_t presentation{
                carrot::scene::make_transition_presentation(snapshot)
            };

            CARROT_TEST_REQUIRE(presentation.visible);
            CARROT_TEST_REQUIRE(!presentation.show_loading_text);
            CARROT_TEST_REQUIRE(presentation.phase_label == "preparing");
            CARROT_TEST_REQUIRE(presentation.overlay_opacity > 0.35f);
            CARROT_TEST_REQUIRE(presentation.progress == 0.25f);
        }

        void test_transition_presentation_uses_black_fade_defaults()
        {
            carrot::scene::scene_runtime_t runtime;
            const carrot::scene::scene_transition_overlay_options_t& options{ runtime.default_transition_overlay_options() };
            const carrot::scene::scene_transition_overlay_options_t& engine_options{ runtime.engine_transition_overlay_options() };

            CARROT_TEST_REQUIRE(options.enabled);
            CARROT_TEST_REQUIRE(options.overlay_color_abgr == 0xFF000000u);
            CARROT_TEST_REQUIRE(options.fade_out_to_black_seconds == 0.5f);
            CARROT_TEST_REQUIRE(options.minimum_opaque_hold_seconds == 0.0f);
            CARROT_TEST_REQUIRE(options.fade_in_from_black_seconds == 0.5f);
            CARROT_TEST_REQUIRE(options.wipe_direction == carrot::scene::scene_transition_wipe_direction_t::left_to_right);
            CARROT_TEST_REQUIRE(!options.show_loading_text);
            CARROT_TEST_REQUIRE(engine_options.overlay_color_abgr == options.overlay_color_abgr);
        }

        void test_scene_runtime_uses_engine_camera_defaults()
        {
            carrot::scene::scene_runtime_t runtime;
            const carrot::scene::scene_camera_options_t& options{ runtime.default_camera_options() };
            const carrot::scene::scene_camera_options_t& engine_options{ runtime.engine_camera_options() };

            CARROT_TEST_REQUIRE(options.projection_mode == carrot::scene::scene_camera_projection_mode_t::orthographic);
            CARROT_TEST_REQUIRE(options.bounds_mode == carrot::scene::scene_camera_bounds_mode_t::none);
            CARROT_TEST_REQUIRE(options.zoom == 4.f);
            CARROT_TEST_REQUIRE(options.follow_mode == carrot::assets::scene_camera_follow_mode_t::player);
            CARROT_TEST_REQUIRE(options.initial_target_policy ==
                                carrot::assets::scene_camera_initial_target_policy_t::player);
            CARROT_TEST_REQUIRE(options.dead_zone_size_world.x == 2.0f);
            CARROT_TEST_REQUIRE(options.dead_zone_size_world.y == 1.5f);
            CARROT_TEST_REQUIRE(options.follow_smoothing == 10.0f);
            CARROT_TEST_REQUIRE(engine_options.zoom == options.zoom);
        }

        void test_scene_runtime_project_default_camera_resolves_over_engine_default()
        {
            carrot::scene::scene_runtime_t runtime;
            runtime.set_default_camera_override(carrot::scene::scene_camera_override_t{
                .bounds_mode = carrot::scene::scene_camera_bounds_mode_t::scene_extents,
                .zoom = 3.0f,
                .follow_mode = carrot::assets::scene_camera_follow_mode_t::none
            });

            const carrot::scene::scene_camera_options_t& options{ runtime.default_camera_options() };
            const carrot::scene::scene_camera_options_t& engine_options{ runtime.engine_camera_options() };

            CARROT_TEST_REQUIRE(options.bounds_mode == carrot::scene::scene_camera_bounds_mode_t::scene_extents);
            CARROT_TEST_REQUIRE(options.zoom == 3.0f);
            CARROT_TEST_REQUIRE(options.follow_mode == carrot::assets::scene_camera_follow_mode_t::none);
            CARROT_TEST_REQUIRE(engine_options.bounds_mode == carrot::scene::scene_camera_bounds_mode_t::none);
            CARROT_TEST_REQUIRE(engine_options.zoom == 4.f);
        }

        void test_scene_camera_override_preserves_unspecified_defaults()
        {
            const carrot::scene::scene_camera_options_t resolved{
                carrot::scene::resolve_scene_camera_options(
                    carrot::scene::scene_camera_options_t{
                        .projection_mode = carrot::scene::scene_camera_projection_mode_t::perspective,
                        .bounds_mode = carrot::scene::scene_camera_bounds_mode_t::scene_extents,
                        .zoom = 2.0f,
                        .follow_mode = carrot::assets::scene_camera_follow_mode_t::player,
                        .initial_target_policy = carrot::assets::scene_camera_initial_target_policy_t::spawn_marker,
                        .dead_zone_size_world = { 5.0f, 4.0f },
                        .follow_smoothing = 6.0f
                    },
                    carrot::scene::scene_camera_override_t{
                        .zoom = 3.5f,
                        .follow_mode = carrot::assets::scene_camera_follow_mode_t::none
                    })
            };

            CARROT_TEST_REQUIRE(resolved.projection_mode == carrot::scene::scene_camera_projection_mode_t::perspective);
            CARROT_TEST_REQUIRE(resolved.bounds_mode == carrot::scene::scene_camera_bounds_mode_t::scene_extents);
            CARROT_TEST_REQUIRE(resolved.zoom == 3.5f);
            CARROT_TEST_REQUIRE(resolved.follow_mode == carrot::assets::scene_camera_follow_mode_t::none);
            CARROT_TEST_REQUIRE(resolved.initial_target_policy ==
                                carrot::assets::scene_camera_initial_target_policy_t::spawn_marker);
            CARROT_TEST_REQUIRE(resolved.dead_zone_size_world.x == 5.0f);
            CARROT_TEST_REQUIRE(resolved.dead_zone_size_world.y == 4.0f);
            CARROT_TEST_REQUIRE(resolved.follow_smoothing == 6.0f);
        }

        void test_scene_runtime_can_disable_project_default_transition_overlay()
        {
            carrot::scene::scene_runtime_t runtime;
            runtime.set_default_transition_overlay_override(carrot::scene::scene_transition_overlay_override_t{
                .style = carrot::scene::scene_transition_overlay_style_t::none
            });

            CARROT_TEST_REQUIRE(!runtime.default_transition_overlay_options().enabled);
            CARROT_TEST_REQUIRE(runtime.engine_transition_overlay_options().enabled);
        }

        void test_scene_runtime_project_default_resolves_over_engine_default()
        {
            carrot::scene::scene_runtime_t runtime;
            runtime.set_default_transition_overlay_override(carrot::scene::scene_transition_overlay_override_t{
                .style = carrot::scene::scene_transition_overlay_style_t::loading_screen,
                .loading_title_text = std::string{ "Project Default" },
                .show_progress_text = true,
                .wipe_direction = carrot::scene::scene_transition_wipe_direction_t::bottom_to_top
            });

            const carrot::scene::scene_transition_overlay_options_t& defaults{
                runtime.default_transition_overlay_options()
            };
            CARROT_TEST_REQUIRE(defaults.enabled);
            CARROT_TEST_REQUIRE(defaults.style == carrot::scene::scene_transition_overlay_style_t::loading_screen);
            CARROT_TEST_REQUIRE(defaults.wipe_direction == carrot::scene::scene_transition_wipe_direction_t::bottom_to_top);
            CARROT_TEST_REQUIRE(defaults.loading_title_text == "Project Default");
            CARROT_TEST_REQUIRE(defaults.show_progress_text);
        }

        void test_transition_overlay_override_none_disables_defaults()
        {
            const carrot::scene::scene_transition_overlay_options_t resolved{
                carrot::scene::resolve_transition_overlay_options(
                    carrot::scene::scene_transition_overlay_options_t{},
                    carrot::scene::scene_transition_overlay_override_t{
                        .style = carrot::scene::scene_transition_overlay_style_t::none
                    }
                )
            };

            CARROT_TEST_REQUIRE(!resolved.enabled);
            CARROT_TEST_REQUIRE(resolved.effect == carrot::scene::scene_transition_effect_t::none);
            CARROT_TEST_REQUIRE(resolved.overlay_color_abgr == 0xFF000000u);
        }

        void test_transition_overlay_override_fade_applies_requested_fields()
        {
            const carrot::scene::scene_transition_overlay_options_t resolved{
                carrot::scene::resolve_transition_overlay_options(
                    carrot::scene::scene_transition_overlay_options_t{
                        .enabled = false,
                        .overlay_color_abgr = 0xFF000000u,
                        .fade_out_to_black_seconds = 0.5f,
                        .minimum_opaque_hold_seconds = 0.0f,
                        .fade_in_from_black_seconds = 0.5f,
                        .show_loading_text = false
                    },
                    carrot::scene::scene_transition_overlay_override_t{
                        .style = carrot::scene::scene_transition_overlay_style_t::fade,
                        .overlay_color_abgr = 0xFF112233u,
                        .fade_out_to_black_seconds = 0.25f,
                        .minimum_opaque_hold_seconds = 0.1f,
                        .fade_in_from_black_seconds = 0.75f,
                        .show_loading_text = true
                    }
                )
            };

            CARROT_TEST_REQUIRE(resolved.enabled);
            CARROT_TEST_REQUIRE(resolved.effect == carrot::scene::scene_transition_effect_t::fade);
            CARROT_TEST_REQUIRE(resolved.overlay_color_abgr == 0xFF112233u);
            CARROT_TEST_REQUIRE(resolved.fade_out_to_black_seconds == 0.25f);
            CARROT_TEST_REQUIRE(resolved.minimum_opaque_hold_seconds == 0.1f);
            CARROT_TEST_REQUIRE(resolved.fade_in_from_black_seconds == 0.75f);
            CARROT_TEST_REQUIRE(resolved.show_loading_text);
        }

        void test_transition_overlay_override_loading_screen_applies_text_configuration()
        {
            const carrot::scene::scene_transition_overlay_options_t resolved{
                carrot::scene::resolve_transition_overlay_options(
                    carrot::scene::scene_transition_overlay_options_t{},
                    carrot::scene::scene_transition_overlay_override_t{
                        .style = carrot::scene::scene_transition_overlay_style_t::loading_screen,
                        .loading_title_text = std::string{ "Entering Inn" },
                        .loading_subtitle_text = std::string{ "Please wait" },
                        .show_progress_text = true,
                        .loading_text_color_abgr = 0xFFEEDDCCu,
                        .loading_subtext_color_abgr = 0xFFBBAA99u
                    }
                )
            };

            CARROT_TEST_REQUIRE(resolved.enabled);
            CARROT_TEST_REQUIRE(resolved.effect == carrot::scene::scene_transition_effect_t::loading_screen);
            CARROT_TEST_REQUIRE(resolved.style == carrot::scene::scene_transition_overlay_style_t::loading_screen);
            CARROT_TEST_REQUIRE(resolved.show_loading_text);
            CARROT_TEST_REQUIRE(resolved.show_progress_text);
            CARROT_TEST_REQUIRE(resolved.loading_title_text == "Entering Inn");
            CARROT_TEST_REQUIRE(resolved.loading_subtitle_text == "Please wait");
            CARROT_TEST_REQUIRE(resolved.loading_text_color_abgr == 0xFFEEDDCCu);
            CARROT_TEST_REQUIRE(resolved.loading_subtext_color_abgr == 0xFFBBAA99u);
        }

        void test_transition_overlay_override_wipe_selects_wipe_style()
        {
            const carrot::scene::scene_transition_overlay_options_t resolved{
                carrot::scene::resolve_transition_overlay_options(
                    carrot::scene::scene_transition_overlay_options_t{},
                    carrot::scene::scene_transition_overlay_override_t{
                        .style = carrot::scene::scene_transition_overlay_style_t::wipe,
                        .wipe_direction = carrot::scene::scene_transition_wipe_direction_t::right_to_left,
                        .overlay_color_abgr = 0xFF224466u
                    }
                )
            };

            CARROT_TEST_REQUIRE(resolved.enabled);
            CARROT_TEST_REQUIRE(resolved.effect == carrot::scene::scene_transition_effect_t::wipe);
            CARROT_TEST_REQUIRE(resolved.style == carrot::scene::scene_transition_overlay_style_t::wipe);
            CARROT_TEST_REQUIRE(resolved.wipe_direction == carrot::scene::scene_transition_wipe_direction_t::right_to_left);
            CARROT_TEST_REQUIRE(resolved.overlay_color_abgr == 0xFF224466u);
            CARROT_TEST_REQUIRE(!resolved.show_loading_text);
        }

        void test_transition_effect_override_preserves_named_effect_identity()
        {
            const carrot::scene::scene_transition_overlay_options_t resolved{
                carrot::scene::resolve_transition_overlay_options(
                    carrot::scene::scene_transition_overlay_options_t{},
                    carrot::scene::scene_transition_overlay_override_t{
                        .effect = carrot::scene::scene_transition_effect_t::battle_swirl,
                        .overlay_color_abgr = 0xFF446688u
                    }
                )
            };

            CARROT_TEST_REQUIRE(resolved.enabled);
            CARROT_TEST_REQUIRE(resolved.effect == carrot::scene::scene_transition_effect_t::battle_swirl);
            CARROT_TEST_REQUIRE(resolved.style == carrot::scene::scene_transition_overlay_style_t::fade);
            CARROT_TEST_REQUIRE(resolved.overlay_color_abgr == 0xFF446688u);
        }

        void test_renderer_composite_overlay_routes_to_composite_stage()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t gfx{ vfs, graphics_config, window::invalid_window_id };

            auto* null_rhi{ dynamic_cast<rhi::null::null_rhi_context_t*>(gfx.get_rhi()) };
            CARROT_TEST_REQUIRE(null_rhi != nullptr);

            gfx.begin_frame();
            CARROT_TEST_REQUIRE(gfx.pending_composite_fullscreen_pass_count() == 0u);
            gfx.set_composite_overlay_color(0xCC112233u);
            gfx.end_frame();

            const auto& textured_stages{ null_rhi->recorded_textured_stages() };
            const auto& text_stages{ null_rhi->recorded_text_stages() };
            const chlm::uint2 render_target_size{ gfx.current_render_target_pixel_size() };
            const renderer::renderer_stats_t& stats{ gfx.get_last_completed_stats() };

            CARROT_TEST_REQUIRE(textured_stages.size() == 1u);
            CARROT_TEST_REQUIRE(text_stages.empty());
            CARROT_TEST_REQUIRE(textured_stages[0].batch_count == 1u);
            CARROT_TEST_REQUIRE(textured_stages[0].presentation_mask == rhi::presentation_channel_gameplay);
            CARROT_TEST_REQUIRE(textured_stages[0].viewport.rect_px.position.x == 0u);
            CARROT_TEST_REQUIRE(textured_stages[0].viewport.rect_px.position.y == 0u);
            CARROT_TEST_REQUIRE(textured_stages[0].viewport.rect_px.size.x == render_target_size.x);
            CARROT_TEST_REQUIRE(textured_stages[0].viewport.rect_px.size.y == render_target_size.y);
            CARROT_TEST_REQUIRE(textured_stages[0].point_light_count == 0u);
            CARROT_TEST_REQUIRE(textured_stages[0].ambient_color.x == 1.f);
            CARROT_TEST_REQUIRE(textured_stages[0].ambient_color.y == 1.f);
            CARROT_TEST_REQUIRE(textured_stages[0].ambient_color.z == 1.f);
            CARROT_TEST_REQUIRE(textured_stages[0].ambient_color.w == 1.f);
            CARROT_TEST_REQUIRE(stats.composite_fullscreen_pass_count == 1u);

            gfx.begin_frame();
            CARROT_TEST_REQUIRE(gfx.pending_composite_fullscreen_pass_count() == 0u);
        }

        void test_renderer_transition_fade_routes_to_composite_stage()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t gfx{ vfs, graphics_config, window::invalid_window_id };

            auto* null_rhi{ dynamic_cast<rhi::null::null_rhi_context_t*>(gfx.get_rhi()) };
            CARROT_TEST_REQUIRE(null_rhi != nullptr);

            gfx.begin_frame();
            CARROT_TEST_REQUIRE(gfx.pending_composite_fullscreen_pass_count() == 0u);
            gfx.set_transition_fade_color(0xCC223344u);
            gfx.end_frame();

            const auto& textured_stages{ null_rhi->recorded_textured_stages() };
            const auto& text_stages{ null_rhi->recorded_text_stages() };
            const chlm::uint2 render_target_size{ gfx.current_render_target_pixel_size() };
            const renderer::renderer_stats_t& stats{ gfx.get_last_completed_stats() };

            CARROT_TEST_REQUIRE(textured_stages.size() == 1u);
            CARROT_TEST_REQUIRE(text_stages.empty());
            CARROT_TEST_REQUIRE(textured_stages[0].batch_count == 1u);
            CARROT_TEST_REQUIRE(textured_stages[0].presentation_mask == rhi::presentation_channel_gameplay);
            CARROT_TEST_REQUIRE(textured_stages[0].viewport.rect_px.position.x == 0u);
            CARROT_TEST_REQUIRE(textured_stages[0].viewport.rect_px.position.y == 0u);
            CARROT_TEST_REQUIRE(textured_stages[0].viewport.rect_px.size.x == render_target_size.x);
            CARROT_TEST_REQUIRE(textured_stages[0].viewport.rect_px.size.y == render_target_size.y);
            CARROT_TEST_REQUIRE(stats.composite_fullscreen_pass_count == 1u);

            gfx.begin_frame();
            CARROT_TEST_REQUIRE(gfx.pending_composite_fullscreen_pass_count() == 0u);
        }

        void test_renderer_transition_battle_swirl_routes_to_capture_textured_stage()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t gfx{ vfs, graphics_config, window::invalid_window_id };

            auto* null_rhi{ dynamic_cast<rhi::null::null_rhi_context_t*>(gfx.get_rhi()) };
            CARROT_TEST_REQUIRE(null_rhi != nullptr);

            gfx.begin_frame();
            gfx.set_transition_battle_swirl(0.65f, false);
            gfx.end_frame();

            const auto& textured_stages{ null_rhi->recorded_textured_stages() };
            CARROT_TEST_REQUIRE(textured_stages.size() == 1u);
            CARROT_TEST_REQUIRE(textured_stages[0].capture_presentation_before_draw);
            CARROT_TEST_REQUIRE(textured_stages[0].batch_count == 1u);
        }

        void test_renderer_bloom_routes_to_composite_stage_when_enabled()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t gfx{ vfs, graphics_config, window::invalid_window_id };

            auto* null_rhi{ dynamic_cast<rhi::null::null_rhi_context_t*>(gfx.get_rhi()) };
            CARROT_TEST_REQUIRE(null_rhi != nullptr);

            gfx.set_bloom_settings(renderer::bloom_settings_t{
                .enabled = true,
                .baseline_strength = 0.2f,
                .peak_light_response = 0.0f,
                .accumulated_light_response = 0.0f,
                .ambient_response = 0.0f,
                .max_strength = 0.3f,
                .tint_abgr = 0xFFFFFFFFu
            });

            gfx.begin_frame();
            CARROT_TEST_REQUIRE(gfx.pending_composite_fullscreen_pass_count() == 0u);
            gfx.end_frame();

            const auto& textured_stages{ null_rhi->recorded_textured_stages() };
            const renderer::renderer_stats_t& stats{ gfx.get_last_completed_stats() };
            CARROT_TEST_REQUIRE(textured_stages.size() == 1u);
            CARROT_TEST_REQUIRE(stats.composite_fullscreen_pass_count == 1u);
            CARROT_TEST_REQUIRE(stats.bloom_pass_count == 1u);
        }

        void test_renderer_bloom_can_be_disabled()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t gfx{ vfs, graphics_config, window::invalid_window_id };

            auto* null_rhi{ dynamic_cast<rhi::null::null_rhi_context_t*>(gfx.get_rhi()) };
            CARROT_TEST_REQUIRE(null_rhi != nullptr);

            gfx.set_bloom_settings(renderer::bloom_settings_t{
                .enabled = false,
                .baseline_strength = 0.2f,
                .peak_light_response = 0.0f,
                .accumulated_light_response = 0.0f,
                .ambient_response = 0.0f,
                .max_strength = 0.3f,
                .tint_abgr = 0xFFFFFFFFu
            });

            gfx.begin_frame();
            gfx.end_frame();

            const auto& textured_stages{ null_rhi->recorded_textured_stages() };
            const renderer::renderer_stats_t& stats{ gfx.get_last_completed_stats() };
            CARROT_TEST_REQUIRE(textured_stages.empty());
            CARROT_TEST_REQUIRE(stats.composite_fullscreen_pass_count == 0u);
            CARROT_TEST_REQUIRE(stats.bloom_pass_count == 0u);
        }

        void test_renderer_composite_and_overlay_debug_use_distinct_stage_spaces()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t gfx{ vfs, graphics_config, window::invalid_window_id };

            auto* null_rhi{ dynamic_cast<rhi::null::null_rhi_context_t*>(gfx.get_rhi()) };
            CARROT_TEST_REQUIRE(null_rhi != nullptr);

            gfx.get_rhi()->resize(1280u, 800u);
            renderer::camera_2d_t camera{ gfx.get_camera_2d() };
            camera.sizing_mode = renderer::camera_2d_sizing_mode_t::fixed_aspect_letterbox;
            camera.design_view_size = { 1280.f, 720.f };
            gfx.set_camera_2d(camera);

            gfx.begin_frame();
            gfx.draw_composite_solid_quad({
                .x = 0.f,
                .y = 0.f,
                .width = 32.f,
                .height = 32.f,
                .layer = renderer::render_layer_t::ui,
                .color = 0xFFFFFFFFu
            });
            gfx.draw_overlay_solid_quad({
                .x = 0.f,
                .y = 0.f,
                .width = 32.f,
                .height = 32.f,
                .layer = renderer::render_layer_t::debug,
                .color = 0xFFFFFFFFu
            });
            gfx.end_frame();

            const auto& textured_stages{ null_rhi->recorded_textured_stages() };
            CARROT_TEST_REQUIRE(textured_stages.size() == 2u);

            const auto& composite_stage{ textured_stages[0] };
            const auto& overlay_stage{ textured_stages[1] };

            CARROT_TEST_REQUIRE(composite_stage.batch_count == 1u);
            CARROT_TEST_REQUIRE(composite_stage.presentation_mask == rhi::presentation_channel_gameplay);
            CARROT_TEST_REQUIRE(composite_stage.viewport.rect_px.position.x == 0u);
            CARROT_TEST_REQUIRE(composite_stage.viewport.rect_px.position.y == 0u);
            CARROT_TEST_REQUIRE(composite_stage.viewport.rect_px.size.x == 1280u);
            CARROT_TEST_REQUIRE(composite_stage.viewport.rect_px.size.y == 800u);

            CARROT_TEST_REQUIRE(overlay_stage.batch_count == 1u);
            CARROT_TEST_REQUIRE(overlay_stage.presentation_mask == rhi::presentation_channel_gameplay);
            CARROT_TEST_REQUIRE(overlay_stage.viewport.rect_px.position.x == 0u);
            CARROT_TEST_REQUIRE(overlay_stage.viewport.rect_px.position.y == 40u);
            CARROT_TEST_REQUIRE(overlay_stage.viewport.rect_px.size.x == 1280u);
            CARROT_TEST_REQUIRE(overlay_stage.viewport.rect_px.size.y == 720u);
        }

        void test_renderer_ui_and_log_console_text_use_distinct_presentation_channels()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t gfx{ vfs, graphics_config, window::invalid_window_id };
            auto* null_rhi{ dynamic_cast<rhi::null::null_rhi_context_t*>(gfx.get_rhi()) };
            CARROT_TEST_REQUIRE(null_rhi != nullptr);

            std::unique_ptr<rhi::rhi_texture_t> texture{
                gfx.get_rhi()->create_texture_2d(rhi::texture_create_info_t{
                    .width = 64u,
                    .height = 64u,
                    .format = rhi::texture_format_t::rgba8_srgb
                })
            };
            CARROT_TEST_REQUIRE(texture != nullptr);

            gfx.begin_frame();
            gfx.draw_ui_text_quad({
                .texture = texture.get(),
                .x = 8.f,
                .y = 8.f,
                .width = 24.f,
                .height = 12.f
            });
            gfx.draw_log_console_text_quad({
                .texture = texture.get(),
                .x = 10.f,
                .y = 10.f,
                .width = 24.f,
                .height = 12.f
            });
            gfx.end_frame();

            const auto& text_stages{ null_rhi->recorded_text_stages() };
            CARROT_TEST_REQUIRE(text_stages.size() == 2u);
            CARROT_TEST_REQUIRE(text_stages[0].presentation_mask == rhi::presentation_channel_gameplay);
            CARROT_TEST_REQUIRE(text_stages[1].presentation_mask == rhi::presentation_channel_log_console);
        }

        void test_renderer_presentation_window_registration_delegates_to_rhi()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t gfx{ vfs, graphics_config, window::invalid_window_id };
            auto* null_rhi{ dynamic_cast<rhi::null::null_rhi_context_t*>(gfx.get_rhi()) };
            CARROT_TEST_REQUIRE(null_rhi != nullptr);

            const window::window_id_t gameplay_mirror_id{ static_cast<window::window_id_t>(101u) };
            const window::window_id_t log_console_id{ static_cast<window::window_id_t>(202u) };

            CARROT_TEST_REQUIRE(gfx.add_presentation_window(gameplay_mirror_id, rhi::presentation_channel_gameplay));
            CARROT_TEST_REQUIRE(gfx.add_presentation_window(log_console_id, rhi::presentation_channel_log_console));
            CARROT_TEST_REQUIRE(!gfx.add_presentation_window(log_console_id, rhi::presentation_channel_log_console));

            const auto& windows_after_add{ null_rhi->registered_presentation_windows() };
            CARROT_TEST_REQUIRE(windows_after_add.size() == 2u);
            CARROT_TEST_REQUIRE(windows_after_add[0].window_id == gameplay_mirror_id);
            CARROT_TEST_REQUIRE(windows_after_add[0].presentation_channel_mask == rhi::presentation_channel_gameplay);
            CARROT_TEST_REQUIRE(windows_after_add[1].window_id == log_console_id);
            CARROT_TEST_REQUIRE(windows_after_add[1].presentation_channel_mask == rhi::presentation_channel_log_console);

            CARROT_TEST_REQUIRE(gfx.remove_presentation_window(gameplay_mirror_id));
            CARROT_TEST_REQUIRE(!gfx.remove_presentation_window(gameplay_mirror_id));

            const auto& windows_after_remove{ null_rhi->registered_presentation_windows() };
            CARROT_TEST_REQUIRE(windows_after_remove.size() == 1u);
            CARROT_TEST_REQUIRE(windows_after_remove[0].window_id == log_console_id);
        }

        void test_renderer_world_light_overflow_is_visible_in_stats()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t gfx{ vfs, graphics_config, window::invalid_window_id };

            world::world_t world;
            for (std::size_t i{ 0u }; i < renderer::k_max_world_point_lights + 3u; ++i)
            {
                world.lighting().point_lights.push_back(world::world_lighting_state_t::point_light_t{
                    .position_world = { static_cast<float>(i), static_cast<float>(i) },
                    .radius_world = 2.f,
                    .color = { 1.f, 0.8f, 0.6f, 1.f },
                    .intensity = 1.f
                });
            }

            gfx.begin_frame();
            gfx.draw_world(world);
            gfx.end_frame();

            const renderer::renderer_stats_t& stats{ gfx.get_last_completed_stats() };
            CARROT_TEST_REQUIRE(stats.world_point_light_count == static_cast<uint32_t>(renderer::k_max_world_point_lights));
            CARROT_TEST_REQUIRE(stats.dropped_world_point_light_count == 3u);
            CARROT_TEST_REQUIRE(stats.forward_plus_tile_count > 0u);
            CARROT_TEST_REQUIRE(stats.forward_plus_light_index_count > 0u);
            CARROT_TEST_REQUIRE(stats.forward_plus_dropped_light_references == 0u);
        }

        void test_renderer_extracts_world_render_items_before_world_execution()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t gfx{ vfs, graphics_config, window::invalid_window_id };
            auto* null_rhi{ dynamic_cast<rhi::null::null_rhi_context_t*>(gfx.get_rhi()) };
            CARROT_TEST_REQUIRE(null_rhi != nullptr);

            gfx.begin_frame();
            CARROT_TEST_REQUIRE(gfx.pending_world_render_item_count() == 0u);

            gfx.draw_solid_quad({
                .x = 16.f,
                .y = 24.f,
                .width = 32.f,
                .height = 48.f,
                .layer = renderer::render_layer_t::world_front,
                .order_mode = renderer::render_order_mode_t::explicit_order,
                .order_in_layer = 7,
                .color = 0xFF80FFFFu
            });

            CARROT_TEST_REQUIRE(gfx.pending_world_render_item_count() == 1u);
            CARROT_TEST_REQUIRE(null_rhi->recorded_textured_stages().empty());
            CARROT_TEST_REQUIRE(null_rhi->recorded_indirect_textured_stages().empty());

            gfx.end_frame();

            CARROT_TEST_REQUIRE(null_rhi->recorded_textured_stages().empty());
            CARROT_TEST_REQUIRE(!null_rhi->recorded_indirect_textured_stages().empty());

            gfx.begin_frame();
            CARROT_TEST_REQUIRE(gfx.pending_world_render_item_count() == 0u);
        }

        void test_loaded_tilemap_asset_builds_sparse_render_chunks_for_tile_layers()
        {
            assets::tilemap_asset_t tilemap;
            tilemap.set_size(32u, 32u);

            assets::tilemap_layer_t layer;
            layer.kind = assets::tilemap_layer_kind_t::tile;
            layer.width = 32u;
            layer.height = 32u;
            layer.gids.resize(layer.width * layer.height, 0u);
            layer.gids[0u] = 1u;
            layer.gids[17u] = 2u;
            layer.gids[(20u * layer.width) + 20u] = 3u;
            tilemap.add_layer(std::move(layer));

            assets::loaded_tilemap_asset_t loaded{ std::move(tilemap), nullptr };
            const auto chunks{ loaded.tile_render_chunks_for_layer(0u) };

            CARROT_TEST_REQUIRE(chunks.size() == 3u);
            CARROT_TEST_REQUIRE(chunks[0].chunk_x == 0u);
            CARROT_TEST_REQUIRE(chunks[0].chunk_y == 0u);
            CARROT_TEST_REQUIRE(chunks[0].occupied_cell_indices.size() == 1u);
            CARROT_TEST_REQUIRE(chunks[1].chunk_x == 1u);
            CARROT_TEST_REQUIRE(chunks[1].chunk_y == 0u);
            CARROT_TEST_REQUIRE(chunks[1].occupied_cell_indices.size() == 1u);
            CARROT_TEST_REQUIRE(chunks[2].chunk_x == 1u);
            CARROT_TEST_REQUIRE(chunks[2].chunk_y == 1u);
            CARROT_TEST_REQUIRE(chunks[2].occupied_cell_indices.size() == 1u);
        }

        void test_renderer_dispatches_world_item_cull_compute_for_world_items()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t gfx{ vfs, graphics_config, window::invalid_window_id };
            auto* null_rhi{ dynamic_cast<rhi::null::null_rhi_context_t*>(gfx.get_rhi()) };
            CARROT_TEST_REQUIRE(null_rhi != nullptr);

            gfx.begin_frame();
            gfx.draw_solid_quad({
                .x = 12.f,
                .y = 20.f,
                .width = 24.f,
                .height = 24.f,
                .layer = renderer::render_layer_t::world_front
            });
            gfx.end_frame();

            const auto& dispatches{ null_rhi->recorded_compute_dispatches() };
            const auto cull_it{
                std::find_if(dispatches.begin(),
                             dispatches.end(),
                             [](const rhi::null::null_rhi_context_t::recorded_compute_dispatch_t& dispatch)
                             {
                                 return dispatch.debug_name == "world item cull";
                             })
            };

            CARROT_TEST_REQUIRE(cull_it != dispatches.end());
            CARROT_TEST_REQUIRE(cull_it->read_only_buffer_count == 2u);
            CARROT_TEST_REQUIRE(cull_it->storage_buffer_count == 2u);
            CARROT_TEST_REQUIRE(cull_it->constant_size_bytes == 0u);
            CARROT_TEST_REQUIRE(cull_it->group_count_x == 1u);
        }

        void test_renderer_records_indirect_world_stage_for_world_items()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t gfx{ vfs, graphics_config, window::invalid_window_id };
            auto* null_rhi{ dynamic_cast<rhi::null::null_rhi_context_t*>(gfx.get_rhi()) };
            CARROT_TEST_REQUIRE(null_rhi != nullptr);

            gfx.begin_frame();
            gfx.draw_solid_quad({
                .x = 32.f,
                .y = 48.f,
                .width = 64.f,
                .height = 64.f,
                .layer = renderer::render_layer_t::world_front,
                .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
            });
            gfx.end_frame();

            CARROT_TEST_REQUIRE(null_rhi->recorded_textured_stages().empty());
            CARROT_TEST_REQUIRE(null_rhi->recorded_indirect_textured_stages().size() == 1u);
            CARROT_TEST_REQUIRE(null_rhi->recorded_indirect_textured_stages()[0].point_light_count == 0u);
        }

        void test_interaction_outcome_dispatch_routes_scene_transition_and_container()
        {
            bool transition_called{ false };
            bool container_called{ false };
            carrot::scene::scene_transition_request_t seen_transition;
            carrot::world::world_object_id_t seen_object_id{ 0 };
            std::string seen_loot_table;

            CARROT_TEST_REQUIRE(carrot::world::authored::dispatch_interaction_outcome(
                carrot::world::authored::interaction_outcome_t{
                    .kind = carrot::world::authored::interaction_outcome_kind_t::scene_transition,
                    .transition = {
                        .scene_id = "scene.test.target",
                        .marker_name = "DoorSpawn"
                    }
                },
                carrot::world::authored::interaction_outcome_dispatch_t{
                    .on_scene_transition = [&](const carrot::scene::scene_transition_request_t& request)
                    {
                        transition_called = true;
                        seen_transition = request;
                    }
                }
            ));

            CARROT_TEST_REQUIRE(transition_called);
            CARROT_TEST_REQUIRE(seen_transition.scene_id == "scene.test.target");
            CARROT_TEST_REQUIRE(seen_transition.marker_name == "DoorSpawn");

            CARROT_TEST_REQUIRE(carrot::world::authored::dispatch_interaction_outcome(
                carrot::world::authored::interaction_outcome_t{
                    .kind = carrot::world::authored::interaction_outcome_kind_t::container,
                    .object_id = 77,
                    .loot_table = "loot.basic"
                },
                carrot::world::authored::interaction_outcome_dispatch_t{
                    .on_container = [&](const carrot::world::world_object_id_t object_id, const std::string_view loot_table)
                    {
                        container_called = true;
                        seen_object_id = object_id;
                        seen_loot_table = std::string{ loot_table };
                    }
                }
            ));

            CARROT_TEST_REQUIRE(container_called);
            CARROT_TEST_REQUIRE(seen_object_id == 77);
            CARROT_TEST_REQUIRE(seen_loot_table == "loot.basic");
        }

        void test_scene_runtime_rejects_overlapping_load_requests()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            CARROT_TEST_REQUIRE(runtime.request_load(game, "scene.sandbox.town"));
            CARROT_TEST_REQUIRE(!runtime.request_load(game, "scene.sandbox.inn"));
            CARROT_TEST_REQUIRE(!runtime.request_transition(game, scene::scene_transition_request_t{
                                     .scene_id = "scene.sandbox.inn",
                                     .marker_name = "PlayerSpawn"
                                 }));
        }

        void test_scene_runtime_listener_sees_no_current_context_on_first_load()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            recording_scene_runtime_listener_t listener;
            CARROT_TEST_REQUIRE(runtime.load(game,
                                             "scene.sandbox.town",
                                             scene::scene_load_options_t{
                                                 .listener = &listener,
                                                 .apply_scene_music = false,
                                                 .transition_overlay = {
                                                     .style = scene::scene_transition_overlay_style_t::none
                                                 }
                                             }));

            CARROT_TEST_REQUIRE(listener.before_call_count == 1u);
            CARROT_TEST_REQUIRE(listener.after_call_count == 1u);
            CARROT_TEST_REQUIRE(!listener.before_had_current_context);
            CARROT_TEST_REQUIRE(listener.before_current_scene_id.empty());
            CARROT_TEST_REQUIRE(listener.before_current_spawn_marker.empty());
            CARROT_TEST_REQUIRE(listener.before_next_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(listener.before_next_spawn_marker == "PlayerSpawn");
            CARROT_TEST_REQUIRE(listener.after_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(listener.after_spawn_marker == "PlayerSpawn");
        }

        void test_scene_runtime_listener_sees_previous_context_before_transition_and_active_context_after()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            CARROT_TEST_REQUIRE(runtime.load(game,
                                             "scene.sandbox.town",
                                             scene::scene_load_options_t{
                                                 .apply_scene_music = false,
                                                 .transition_overlay = {
                                                     .style = scene::scene_transition_overlay_style_t::none
                                                 }
                                             }));

            recording_scene_runtime_listener_t listener;
            CARROT_TEST_REQUIRE(runtime.transition(game,
                                                   scene::scene_transition_request_t{
                                                       .scene_id = "scene.sandbox.inn",
                                                       .marker_name = "EntryFromTown"
                                                   },
                                                   scene::scene_load_options_t{
                                                       .listener = &listener,
                                                       .apply_scene_music = false,
                                                       .transition_overlay = {
                                                           .style = scene::scene_transition_overlay_style_t::none
                                                       }
                                                   }));

            CARROT_TEST_REQUIRE(listener.before_call_count == 1u);
            CARROT_TEST_REQUIRE(listener.after_call_count == 1u);
            CARROT_TEST_REQUIRE(listener.before_had_current_context);
            CARROT_TEST_REQUIRE(listener.before_current_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(listener.before_current_spawn_marker == "PlayerSpawn");
            CARROT_TEST_REQUIRE(listener.before_next_scene_id == "scene.sandbox.inn");
            CARROT_TEST_REQUIRE(listener.before_next_spawn_marker == "EntryFromTown");
            CARROT_TEST_REQUIRE(listener.after_scene_id == "scene.sandbox.inn");
            CARROT_TEST_REQUIRE(listener.after_spawn_marker == "EntryFromTown");
        }

        void test_scene_runtime_after_listener_sees_post_activation_runtime_state()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            world::player_controller_t player_controller;
            world::interaction_controller_t interaction_controller;
            post_activation_scene_runtime_listener_t listener;
            listener.player_controller = &player_controller;
            listener.interaction_controller = &interaction_controller;

            scene::scene_runtime_t runtime;
            CARROT_TEST_REQUIRE(runtime.load(game,
                                             "scene.sandbox.town",
                                             carrot::scene::scene_load_options_t{
                                                 .player_controller = &player_controller,
                                                 .interaction_controller = &interaction_controller,
                                                 .listener = &listener,
                                                 .apply_scene_music = false,
                                                 .camera_override = {
                                                     .initial_target_policy =
                                                         assets::scene_camera_initial_target_policy_t::spawn_marker
                                                 },
                                                 .transition_overlay = {
                                                     .style = carrot::scene::scene_transition_overlay_style_t::none
                                                 }
                                             }));

            CARROT_TEST_REQUIRE(listener.after_call_count == 1u);
            CARROT_TEST_REQUIRE(listener.after_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(listener.after_spawn_marker == "PlayerSpawn");
            CARROT_TEST_REQUIRE(listener.after_player_name == "Vraden");
            CARROT_TEST_REQUIRE(listener.after_spawn_name == "PlayerSpawn");
            CARROT_TEST_REQUIRE(listener.after_player_controller_object == "Vraden");
            CARROT_TEST_REQUIRE(listener.after_interaction_actor_object == "Vraden");
            CARROT_TEST_REQUIRE(listener.after_player_controller_matches_context_player);
            CARROT_TEST_REQUIRE(listener.after_interaction_actor_matches_player);
            CARROT_TEST_REQUIRE(listener.after_camera_center_world.x == listener.expected_camera_center_world.x);
            CARROT_TEST_REQUIRE(listener.after_camera_center_world.y == listener.expected_camera_center_world.y);

            const scene::scene_runtime_summary_t summary{ runtime.summarize(game) };
            CARROT_TEST_REQUIRE(summary.player_object_name == "Vraden");
            CARROT_TEST_REQUIRE(summary.spawn_object_name == "PlayerSpawn");
            CARROT_TEST_REQUIRE(summary.player_object_id == player_controller.controlled_object()->id);
        }

        void test_scene_runtime_rebuild_current_scene_requires_active_scene()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            CARROT_TEST_REQUIRE(!runtime.request_rebuild_current_scene(game));
            CARROT_TEST_REQUIRE(!runtime.rebuild_current_scene(game));
        }

        void test_game_runtime_debug_toggles_drive_world_overlay_state()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            core::game_runtime_t runtime{ game };

            CARROT_TEST_REQUIRE(!runtime.map_collision_debug_visible());
            CARROT_TEST_REQUIRE(!runtime.object_collider_debug_visible());
            CARROT_TEST_REQUIRE(!runtime.trigger_volume_debug_visible());

            CARROT_TEST_REQUIRE(runtime.toggle_map_collision_debug());
            CARROT_TEST_REQUIRE(runtime.toggle_object_collider_debug());
            CARROT_TEST_REQUIRE(runtime.toggle_trigger_volume_debug());

            CARROT_TEST_REQUIRE(world.collision_debug_view().show_map_collision);
            CARROT_TEST_REQUIRE(world.collision_debug_view().show_object_colliders);
            CARROT_TEST_REQUIRE(world.collision_debug_view().show_trigger_volumes);

            runtime.set_map_collision_debug_visible(false);
            runtime.set_object_collider_debug_visible(false);
            runtime.set_trigger_volume_debug_visible(false);

            CARROT_TEST_REQUIRE(!runtime.map_collision_debug_visible());
            CARROT_TEST_REQUIRE(!runtime.object_collider_debug_visible());
            CARROT_TEST_REQUIRE(!runtime.trigger_volume_debug_visible());
        }

        void test_scene_runtime_can_rebuild_current_scene()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            CARROT_TEST_REQUIRE(runtime.load(game,
                                             "scene.sandbox.town",
                                             carrot::scene::scene_load_options_t{
                                                 .apply_scene_music = false,
                                                 .transition_overlay = {
                                                     .style = carrot::scene::scene_transition_overlay_style_t::none
                                                 }
                                             }));
            CARROT_TEST_REQUIRE(runtime.has_scene_loaded());
            CARROT_TEST_REQUIRE(runtime.current_scene_id() == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(runtime.request_rebuild_current_scene(game));
            CARROT_TEST_REQUIRE(runtime.has_pending_scene());
            CARROT_TEST_REQUIRE(runtime.pending_scene_id() == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(runtime.pending_spawn_marker() == "PlayerSpawn");

            const carrot::scene::scene_runtime_snapshot_t pending_snapshot{ runtime.snapshot() };
            CARROT_TEST_REQUIRE(pending_snapshot.runtime_state == carrot::scene::scene_runtime_state_t::transitioning);
            CARROT_TEST_REQUIRE(pending_snapshot.pending_request_kind == carrot::scene::scene_change_request_kind_t::rebuild);
            CARROT_TEST_REQUIRE(pending_snapshot.active_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(pending_snapshot.active_spawn_marker == "PlayerSpawn");
            CARROT_TEST_REQUIRE(pending_snapshot.pending_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(pending_snapshot.pending_spawn_marker == "PlayerSpawn");
            CARROT_TEST_REQUIRE(pending_snapshot.is_transitioning());

            while (runtime.has_pending_scene())
                CARROT_TEST_REQUIRE(runtime.update(game));
            CARROT_TEST_REQUIRE(runtime.has_scene_loaded());
            CARROT_TEST_REQUIRE(!runtime.has_pending_scene());
            CARROT_TEST_REQUIRE(runtime.current_scene_id() == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(runtime.current_spawn_marker() == "PlayerSpawn");
            CARROT_TEST_REQUIRE(runtime.runtime_state() == carrot::scene::scene_runtime_state_t::active);
            CARROT_TEST_REQUIRE(runtime.transition_phase() == carrot::scene::scene_transition_phase_t::none);

            const carrot::scene::scene_runtime_snapshot_t complete_snapshot{ runtime.snapshot() };
            CARROT_TEST_REQUIRE(complete_snapshot.pending_request_kind == carrot::scene::scene_change_request_kind_t::none);
        }

        void test_scene_runtime_asset_driven_rebuild_carries_structural_refresh_context()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            CARROT_TEST_REQUIRE(runtime.load(game,
                                             "scene.sandbox.town",
                                             carrot::scene::scene_load_options_t{
                                                 .apply_scene_music = false,
                                                 .transition_overlay = {
                                                     .style = carrot::scene::scene_transition_overlay_style_t::none
                                                 }
                                             }));

            carrot::assets::asset_iteration_status_t status;
            status.kind = carrot::assets::asset_kind_t::tilemap;
            status.logical_id = "tilemap.test.town";
            status.reload_policy = carrot::assets::asset_reload_policy_t::restart_or_scene_rebuild_required;
            status.dependency_shape = carrot::assets::asset_dependency_shape_t::scene_or_world_structure;

            CARROT_TEST_REQUIRE(runtime.request_rebuild_current_scene_for_asset(game, status));
            CARROT_TEST_REQUIRE(runtime.has_pending_scene());

            const carrot::scene::scene_runtime_summary_t pending_summary{ runtime.summarize(game) };
            CARROT_TEST_REQUIRE(pending_summary.diagnostics.has_structural_refresh_context());
            CARROT_TEST_REQUIRE(pending_summary.diagnostics.structural_refresh_asset_kind ==
                                carrot::assets::asset_kind_t::tilemap);
            CARROT_TEST_REQUIRE(pending_summary.diagnostics.structural_refresh_asset_logical_id == "tilemap.test.town");
            CARROT_TEST_REQUIRE(pending_summary.diagnostics.structural_refresh_reason.find("scene/world structure") !=
                                std::string::npos);

            while (runtime.has_pending_scene())
                CARROT_TEST_REQUIRE(runtime.update(game));

            const carrot::scene::scene_runtime_summary_t complete_summary{ runtime.summarize(game) };
            CARROT_TEST_REQUIRE(complete_summary.diagnostics.has_structural_refresh_context());
            CARROT_TEST_REQUIRE(complete_summary.diagnostics.structural_refresh_asset_kind ==
                                carrot::assets::asset_kind_t::tilemap);
            CARROT_TEST_REQUIRE(complete_summary.diagnostics.structural_refresh_asset_logical_id == "tilemap.test.town");
        }

        void test_scene_runtime_asset_driven_rebuild_rejects_non_structural_asset()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            scene::scene_runtime_t runtime;
            CARROT_TEST_REQUIRE(runtime.load(game,
                                             "scene.sandbox.town",
                                             carrot::scene::scene_load_options_t{
                                                 .apply_scene_music = false,
                                                 .transition_overlay = {
                                                     .style = carrot::scene::scene_transition_overlay_style_t::none
                                                 }
                                             }));

            carrot::assets::asset_iteration_status_t status;
            status.kind = carrot::assets::asset_kind_t::texture;
            status.logical_id = "engine.carrot_engine_logo_512";
            status.reload_policy = carrot::assets::asset_reload_policy_t::reloadable_live;
            status.dependency_shape = carrot::assets::asset_dependency_shape_t::leaf_runtime_data;
            status.loaded_in_runtime_cache = true;

            CARROT_TEST_REQUIRE(!runtime.request_rebuild_current_scene_for_asset(game, status));
            CARROT_TEST_REQUIRE(!runtime.has_pending_scene());

            const carrot::scene::scene_runtime_summary_t summary{ runtime.summarize(game) };
            CARROT_TEST_REQUIRE(!summary.diagnostics.has_structural_refresh_context());
        }

        void test_scene_runtime_failed_rebuild_preserves_active_scene()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };

            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            register_required_assets(assets, vfs);

            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            g_rebuild_validation_should_fail = false;
            g_rebuild_validation_call_count = 0u;

            scene::scene_runtime_t runtime;
            CARROT_TEST_REQUIRE(runtime.load(game,
                                             "scene.sandbox.town",
                                             carrot::scene::scene_load_options_t{
                                                 .validate_loaded_scene = rebuild_validation_callback,
                                                 .apply_scene_music = false,
                                                 .transition_overlay = {
                                                     .style = carrot::scene::scene_transition_overlay_style_t::none
                                                 }
                                             }));
            CARROT_TEST_REQUIRE(g_rebuild_validation_call_count == 1u);
            CARROT_TEST_REQUIRE(runtime.has_scene_loaded());
            CARROT_TEST_REQUIRE(runtime.current_scene_id() == "scene.sandbox.town");

            const world::world_object_t* player_before{ game.world.find_object_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player_before != nullptr);
            const world::world_object_id_t player_id_before{ player_before->id };

            g_rebuild_validation_should_fail = true;
            CARROT_TEST_REQUIRE(runtime.request_rebuild_current_scene(game));
            CARROT_TEST_REQUIRE(runtime.has_pending_scene());

            const carrot::scene::scene_runtime_summary_t pending_summary{ runtime.summarize(game) };
            CARROT_TEST_REQUIRE(pending_summary.snapshot.runtime_state ==
                                carrot::scene::scene_runtime_state_t::transitioning);
            CARROT_TEST_REQUIRE(pending_summary.snapshot.pending_request_kind ==
                                carrot::scene::scene_change_request_kind_t::rebuild);
            CARROT_TEST_REQUIRE(pending_summary.diagnostics.visible);
            CARROT_TEST_REQUIRE(pending_summary.diagnostics.request_kind ==
                                carrot::scene::scene_change_request_kind_t::rebuild);
            CARROT_TEST_REQUIRE(pending_summary.diagnostics.outcome ==
                                carrot::scene::scene_change_outcome_t::in_progress);
            CARROT_TEST_REQUIRE(pending_summary.snapshot.active_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(pending_summary.snapshot.pending_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(pending_summary.snapshot.active_spawn_marker == "PlayerSpawn");
            CARROT_TEST_REQUIRE(pending_summary.snapshot.pending_spawn_marker == "PlayerSpawn");
            CARROT_TEST_REQUIRE(pending_summary.diagnostics.target_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(pending_summary.diagnostics.target_spawn_marker == "PlayerSpawn");

            while (runtime.has_pending_scene())
                CARROT_TEST_REQUIRE(runtime.update(game));

            CARROT_TEST_REQUIRE(g_rebuild_validation_call_count == 2u);
            CARROT_TEST_REQUIRE(runtime.has_scene_loaded());
            CARROT_TEST_REQUIRE(!runtime.has_pending_scene());
            CARROT_TEST_REQUIRE(runtime.current_scene_id() == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(runtime.current_spawn_marker() == "PlayerSpawn");
            CARROT_TEST_REQUIRE(runtime.runtime_state() == carrot::scene::scene_runtime_state_t::active);
            CARROT_TEST_REQUIRE(runtime.transition_phase() == carrot::scene::scene_transition_phase_t::none);
            CARROT_TEST_REQUIRE(!runtime.last_scene_change_succeeded());

            const carrot::scene::scene_runtime_snapshot_t failed_snapshot{ runtime.snapshot() };
            CARROT_TEST_REQUIRE(failed_snapshot.pending_request_kind ==
                                carrot::scene::scene_change_request_kind_t::none);
            CARROT_TEST_REQUIRE(failed_snapshot.active_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(failed_snapshot.active_spawn_marker == "PlayerSpawn");
            CARROT_TEST_REQUIRE(!failed_snapshot.has_pending_scene());

            const carrot::scene::scene_runtime_summary_t failed_summary{ runtime.summarize(game) };
            CARROT_TEST_REQUIRE(failed_summary.diagnostics.visible);
            CARROT_TEST_REQUIRE(failed_summary.diagnostics.request_kind ==
                                carrot::scene::scene_change_request_kind_t::rebuild);
            CARROT_TEST_REQUIRE(failed_summary.diagnostics.outcome ==
                                carrot::scene::scene_change_outcome_t::failed);
            CARROT_TEST_REQUIRE(failed_summary.diagnostics.preserved_active_scene);
            CARROT_TEST_REQUIRE(failed_summary.diagnostics.target_scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(failed_summary.diagnostics.target_spawn_marker == "PlayerSpawn");

            const world::world_object_t* player_after{ game.world.find_object_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player_after != nullptr);
            CARROT_TEST_REQUIRE(player_after->id == player_id_before);

            g_rebuild_validation_should_fail = false;
        }

        void test_incremental_scene_load_task_spreads_work_across_multiple_advances()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::scene_load_task_t task{ "scene.sandbox.town" };
            CARROT_TEST_REQUIRE(task.total_steps() >= 5u);
            CARROT_TEST_REQUIRE(task.completed_steps() == 0u);
            CARROT_TEST_REQUIRE(!task.is_ready_to_activate());
            CARROT_TEST_REQUIRE(!task.is_complete());
            CARROT_TEST_REQUIRE(!task.has_failed());

            CARROT_TEST_REQUIRE(task.advance(assets));
            CARROT_TEST_REQUIRE(task.completed_steps() == 1u);
            CARROT_TEST_REQUIRE(!task.is_ready_to_activate());

            size_t advances{ 1u };
            while (!task.is_ready_to_activate() && !task.has_failed())
            {
                CARROT_TEST_REQUIRE(task.advance(assets));
                ++advances;
            }

            CARROT_TEST_REQUIRE(!task.has_failed());
            CARROT_TEST_REQUIRE(task.is_ready_to_activate());
            CARROT_TEST_REQUIRE(advances >= task.total_steps());
            CARROT_TEST_REQUIRE(task.scene_record() != nullptr);
            CARROT_TEST_REQUIRE(task.effective_spawn_marker() == "PlayerSpawn");

            world::world_t staged_world{ task.take_world() };
            CARROT_TEST_REQUIRE(task.is_complete());

            const world::world_object_t* player{ staged_world.find_object_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(player->transform.has_value());

            const world::world_object_t* player_spawn{ staged_world.find_object_by_name("PlayerSpawn") };
            CARROT_TEST_REQUIRE(player_spawn != nullptr);
            CARROT_TEST_REQUIRE(player_spawn->transform.has_value());
            CARROT_TEST_REQUIRE(player->transform->position.x == player_spawn->transform->position.x);
            CARROT_TEST_REQUIRE(player->transform->position.y == player_spawn->transform->position.y);
        }

        void test_incremental_scene_load_task_fails_for_missing_spawn_marker()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::scene_load_task_t task{ "scene.sandbox.town", "MissingSpawn" };
            while (!task.is_ready_to_activate() && !task.has_failed())
                (void)task.advance(assets);

            CARROT_TEST_REQUIRE(task.has_failed());
            CARROT_TEST_REQUIRE(!task.is_ready_to_activate());
        }

        void test_prepared_tilemap_world_data_applies_cleanly_to_world()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.test.overworld") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);

            const world::import::prepared_tilemap_world_data_t prepared{
                world::import::prepare_tilemap_world_data(*tilemap, chlm::float2{ 0.f, 0.f })
            };
            CARROT_TEST_REQUIRE(!prepared.objects.empty());

            world::world_t world;
            const world::import::tilemap_world_bridge_result_t result{
                world::import::apply_prepared_tilemap_world_data(world, *tilemap, prepared)
            };

            CARROT_TEST_REQUIRE(result.markers_created > 0u);
            CARROT_TEST_REQUIRE(result.static_colliders_created == prepared.static_colliders.size());
            CARROT_TEST_REQUIRE(world.objects().size() == prepared.objects.size());
            CARROT_TEST_REQUIRE(world.collision_world().static_colliders().size() == prepared.static_colliders.size());

            const world::world_object_t* marker{ world.find_object_by_name("PlayerSpawn") };
            CARROT_TEST_REQUIRE(marker != nullptr);
            CARROT_TEST_REQUIRE(marker->transform.has_value());
        }

        void test_sandbox_town_player_sprite_exposes_walk_animation()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));

            world::world_object_t* player{ world.find_object_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(player->sprite.has_value());
            CARROT_TEST_REQUIRE(player->sprite_animator.has_value());
            CARROT_TEST_REQUIRE(player->collision.has_value());
            CARROT_TEST_REQUIRE(player->sprite->sprite != nullptr);

            const assets::loaded_sprite_asset_t* sprite{ player->sprite->sprite };
            CARROT_TEST_REQUIRE(sprite == assets.sprites().get("sprite.vraden"));
            CARROT_TEST_REQUIRE(sprite->find_animation("idle_down") != nullptr);
            CARROT_TEST_REQUIRE(sprite->find_animation("walk_right") != nullptr);
        }

        void test_player_controller_can_switch_to_walk_animation_in_sandbox_town()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));

            world::world_object_t* player{ world.find_object_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(player->sprite.has_value());
            CARROT_TEST_REQUIRE(player->sprite_animator.has_value());
            CARROT_TEST_REQUIRE(player->collision.has_value());
            CARROT_TEST_REQUIRE(player->sprite->sprite != nullptr);
            CARROT_TEST_REQUIRE(player->sprite_animator->animator.sprite() == player->sprite->sprite);

            test_player_controller_t controller;
            controller.set_animation_set({
                .idle_down = "idle_down",
                .idle_up = "idle_up",
                .idle_left = "idle_left",
                .idle_right = "idle_right",
                .walk_down = "walk_down",
                .walk_up = "walk_up",
                .walk_left = "walk_left",
                .walk_right = "walk_right"
            });
            controller.set_controlled_object(player);
            player->sprite_animator->animator.play(controller.animation_set().walk_right);
            controller.force_apply_animation(*player, world::facing_direction_t::right, true);

            CARROT_TEST_REQUIRE(player->sprite_animator->animator.sprite() == player->sprite->sprite);
            CARROT_TEST_REQUIRE(player->sprite_animator->animator.current_animation() != nullptr);
            CARROT_TEST_REQUIRE(player->sprite_animator->animator.current_frame() != nullptr);
        }

        void test_sandbox_town_player_animator_can_play_walk_animation()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));

            world::world_object_t* player{ world.find_object_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(player->sprite_animator.has_value());
            CARROT_TEST_REQUIRE(player->sprite.has_value());
            CARROT_TEST_REQUIRE(player->collision.has_value());
            CARROT_TEST_REQUIRE(player->sprite_animator->animator.sprite() == player->sprite->sprite);

            player->sprite_animator->animator.play("walk_right");
            CARROT_TEST_REQUIRE(player->sprite_animator->animator.current_animation() != nullptr);
            CARROT_TEST_REQUIRE(player->sprite_animator->animator.current_frame() != nullptr);
        }

        void test_player_controller_blocks_against_imported_sandbox_town_collision()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));

            world::world_object_t* player{ world.find_object_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(player->transform.has_value());
            CARROT_TEST_REQUIRE(player->collision.has_value());
            CARROT_TEST_REQUIRE(!world.collision_world().static_colliders().empty());

            world::player_controller_t controller;
            controller.set_controlled_object(player);

            const auto& blocker{ world.collision_world().static_colliders().front() };
            const chlm::float2 half_extents{ player->collision->half_extents };
            const chlm::float2 offset{ player->collision->offset };

            player->transform->position = {
                blocker.bounds.min.x - half_extents.x - 0.05f - offset.x,
                blocker.bounds.center().y - offset.y
            };

            const float start_x{ player->transform->position.x };
            const world::player_move_result_t move_result{ controller.move(world, chlm::float2{ 2.f, 0.f }) };
            const collision::collision_aabb_t final_bounds{ controller.current_collision_bounds() };

            CARROT_TEST_REQUIRE(move_result.blocked_x);
            CARROT_TEST_REQUIRE(!move_result.started_overlapping);
            CARROT_TEST_REQUIRE(player->transform->position.x > start_x);
            CARROT_TEST_REQUIRE(final_bounds.max.x <= blocker.bounds.min.x + 1.0e-4f);
            CARROT_TEST_REQUIRE(final_bounds.min.y < blocker.bounds.max.y);
            CARROT_TEST_REQUIRE(final_bounds.max.y > blocker.bounds.min.y);
        }

        void test_player_controller_can_move_away_after_blocking_against_sandbox_town_collision()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));

            world::world_object_t* player{ world.find_object_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(player->transform.has_value());
            CARROT_TEST_REQUIRE(player->collision.has_value());
            CARROT_TEST_REQUIRE(!world.collision_world().static_colliders().empty());

            world::player_controller_t controller;
            controller.set_controlled_object(player);

            const auto& blocker{ world.collision_world().static_colliders().front() };
            const chlm::float2 half_extents{ player->collision->half_extents };
            const chlm::float2 offset{ player->collision->offset };

            player->transform->position = {
                blocker.bounds.min.x - half_extents.x - 0.05f - offset.x,
                blocker.bounds.center().y - offset.y
            };

            const world::player_move_result_t blocked_move{ controller.move(world, chlm::float2{ 2.f, 0.f }) };
            CARROT_TEST_REQUIRE(blocked_move.blocked_x);

            const float blocked_x{ player->transform->position.x };
            const world::player_move_result_t separating_move{ controller.move(world, chlm::float2{ -0.5f, 0.f }) };

            CARROT_TEST_REQUIRE(!separating_move.started_overlapping);
            CARROT_TEST_REQUIRE(!separating_move.blocked_x);
            CARROT_TEST_REQUIRE(player->transform->position.x < blocked_x);
        }

        void test_player_controller_can_escape_small_initial_overlap_in_sandbox_town_collision()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));

            world::world_object_t* player{ world.find_object_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(player->transform.has_value());
            CARROT_TEST_REQUIRE(player->collision.has_value());
            CARROT_TEST_REQUIRE(!world.collision_world().static_colliders().empty());

            world::player_controller_t controller;
            controller.set_controlled_object(player);

            const auto& blocker{ world.collision_world().static_colliders().front() };
            const chlm::float2 half_extents{ player->collision->half_extents };
            const chlm::float2 offset{ player->collision->offset };

            player->transform->position = {
                blocker.bounds.min.x - half_extents.x + 0.02f - offset.x,
                blocker.bounds.center().y - offset.y
            };

            const world::player_move_result_t move_result{ controller.move(world, chlm::float2{ -0.5f, 0.f }) };
            const collision::collision_aabb_t final_bounds{ controller.current_collision_bounds() };

            CARROT_TEST_REQUIRE(move_result.started_overlapping);
            CARROT_TEST_REQUIRE(final_bounds.max.x <= blocker.bounds.min.x + 1.0e-4f);
            CARROT_TEST_REQUIRE(!move_result.blocked_x);
        }

        void test_player_controller_slides_along_vertical_blocker()
        {
            world::world_t world;
            const auto& blocker = world.collision_world().add_static_collider(collision::static_collider_t{
                .bounds = collision::collision_aabb_t::from_min_size(chlm::float2{ 2.f, -1.f }, chlm::float2{ 1.f, 4.f })
            });
            static_cast<void>(blocker);

            world::world_object_t player;
            player.transform = world::transform_component_t{
                .position = { 0.f, 0.2f },
                .scale = { 1.f, 1.f }
            };

            world::player_controller_t controller;
            controller.set_controlled_object(&player);
            player.collision = world::collision_component_t{
                .half_extents = { 0.3f, 0.2f },
                .offset = { 0.f, -0.2f }
            };

            const world::player_move_result_t move_result{ controller.move(world, chlm::float2{ 3.f, 1.f }) };

            CARROT_TEST_REQUIRE(move_result.blocked_x);
            CARROT_TEST_REQUIRE(player.transform->position.y > 0.2f);
            CARROT_TEST_REQUIRE(player.transform->position.x < 2.f);
            CARROT_TEST_REQUIRE(controller.current_collision_bounds().max.x <= 2.f + 1.0e-4f);
        }

        void test_scene_loader_assigns_player_collision_config()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));

            world::world_object_t* player{ world.find_object_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(player->collision.has_value());
            CARROT_TEST_REQUIRE(player->collision->half_extents.x == 0.3f);
            CARROT_TEST_REQUIRE(player->collision->half_extents.y == 0.2f);
            CARROT_TEST_REQUIRE(player->collision->offset.x == 0.f);
            CARROT_TEST_REQUIRE(player->collision->offset.y == -0.2f);
            CARROT_TEST_REQUIRE(player->collision->debug_display.has_value());
            CARROT_TEST_REQUIRE(player->collision->debug_display->color == 0xFF00FFFFu);
        }

        void test_scene_loader_imports_authored_lighting()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));
            const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.sandbox.town") };
            CARROT_TEST_REQUIRE(tilemap != nullptr);

            const auto require_light_object = [tilemap](const std::string_view name) -> const assets::tilemap_object_t& {
                const assets::tilemap_object_t* object{ tilemap->find_object_by_name(name) };
                CARROT_TEST_REQUIRE(object != nullptr);
                return *object;
            };

            const auto ambient_channel = [](const uint8_t channel) {
                return static_cast<float>(channel) / 255.f;
            };

            size_t authored_point_light_count{ 0u };
            for (const assets::tilemap_layer_t& layer : tilemap->tilemap().layers())
            {
                if (layer.kind != assets::tilemap_layer_kind_t::object)
                    continue;

                for (const assets::tilemap_object_t& object : layer.objects)
                {
                    const auto light{ assets::as_typed_light(object) };
                    if (!light || light->kind != assets::typed_light_kind_t::point)
                        continue;

                    ++authored_point_light_count;
                }
            }

            CARROT_TEST_REQUIRE(world.lighting().ambient_color.x == ambient_channel(0x5C));
            CARROT_TEST_REQUIRE(world.lighting().ambient_color.y == ambient_channel(0x61));
            CARROT_TEST_REQUIRE(world.lighting().ambient_color.z == ambient_channel(0x70));
            CARROT_TEST_REQUIRE(world.lighting().point_lights.size() == authored_point_light_count);

            const auto has_authored_point_light = [&world](const assets::tilemap_object_t& object,
                                                           const chlm::float4 color,
                                                           const float radius_world,
                                                           const float intensity) {
                const chlm::float2 expected_position{
                    world::world_units_t::pixels_to_world(object.x),
                    world::world_units_t::pixels_to_world(object.y)
                };
                return std::ranges::any_of(world.lighting().point_lights,
                                           [&](const world::world_lighting_state_t::point_light_t& light) {
                                               return light.behavior == world::world_lighting_state_t::point_light_t::runtime_behavior_t::stationary &&
                                                      std::abs(light.position_world.x - expected_position.x) < 0.001f &&
                                                      std::abs(light.position_world.y - expected_position.y) < 0.001f &&
                                                      std::abs(light.color.x - color.x) < 0.001f &&
                                                      std::abs(light.color.y - color.y) < 0.001f &&
                                                      std::abs(light.color.z - color.z) < 0.001f &&
                                                      std::abs(light.radius_world - radius_world) < 0.001f &&
                                                      std::abs(light.intensity - intensity) < 0.001f;
                                           });
            };

            CARROT_TEST_REQUIRE(has_authored_point_light(require_light_object("BluePointLight"),
                                                         chlm::float4{ ambient_channel(0x33), ambient_channel(0x94), 1.f, 1.f },
                                                         2.5f,
                                                         1.0f));
            CARROT_TEST_REQUIRE(has_authored_point_light(require_light_object("GreenPointLight"),
                                                         chlm::float4{ ambient_channel(0x47), 1.f, ambient_channel(0x6B), 1.f },
                                                         2.35f,
                                                         0.95f));
            CARROT_TEST_REQUIRE(has_authored_point_light(require_light_object("PurplePointLight"),
                                                         chlm::float4{ ambient_channel(0xD1), ambient_channel(0x4D), 1.f, 1.f },
                                                         2.4f,
                                                         1.05f));
            CARROT_TEST_REQUIRE(has_authored_point_light(require_light_object("BigPurpleLight"),
                                                         chlm::float4{ ambient_channel(0xD1), ambient_channel(0x4D), 1.f, 1.f },
                                                         3.25f,
                                                         2.0f));
        }

        void test_scene_loader_follow_light_tracks_player_from_authored_offset()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town", "InnExteriorSpawn"));

            world::world_object_t* player{ world.find_object_by_name("Vraden") };
            CARROT_TEST_REQUIRE(player != nullptr);
            CARROT_TEST_REQUIRE(player->transform.has_value());

            const auto follow_light_it{ std::ranges::find_if(world.lighting().point_lights,
                                                             [](const world::world_lighting_state_t::point_light_t& light) {
                                                                 return light.behavior == world::world_lighting_state_t::point_light_t::runtime_behavior_t::follow_object;
                                                             }) };
            CARROT_TEST_REQUIRE(follow_light_it != world.lighting().point_lights.end());
            CARROT_TEST_REQUIRE(follow_light_it->follow_object_name == "Vraden");
            CARROT_TEST_REQUIRE(std::abs(follow_light_it->follow_offset_world.x) < 0.001f);
            CARROT_TEST_REQUIRE(std::abs(follow_light_it->follow_offset_world.y) < 0.001f);
            CARROT_TEST_REQUIRE(std::abs(follow_light_it->position_world.x - player->transform->position.x) < 0.001f);
            CARROT_TEST_REQUIRE(std::abs(follow_light_it->position_world.y - player->transform->position.y) < 0.001f);

            player->transform->position.x += 1.5f;
            player->transform->position.y -= 0.75f;
            world.refresh_bound_lights();

            const auto updated_follow_light_it{ std::ranges::find_if(world.lighting().point_lights,
                                                                     [](const world::world_lighting_state_t::point_light_t& light) {
                                                                         return light.behavior == world::world_lighting_state_t::point_light_t::runtime_behavior_t::follow_object;
                                                                     }) };
            CARROT_TEST_REQUIRE(updated_follow_light_it != world.lighting().point_lights.end());
            CARROT_TEST_REQUIRE(std::abs(updated_follow_light_it->position_world.x - player->transform->position.x) < 0.001f);
            CARROT_TEST_REQUIRE(std::abs(updated_follow_light_it->position_world.y - player->transform->position.y) < 0.001f);
        }

        void test_scene_loader_imports_authored_trigger_component()
        {
            io::virtual_file_system_t vfs;
            mount_test_asset_roots(vfs);

            fake_context_t rhi;
            assets::asset_manager_t assets{ vfs, rhi };
            register_required_assets(assets, vfs);

            world::world_t world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(world, assets, "scene.sandbox.town"));

            world::world_object_t* trigger{ world.find_object_by_name("Trigger") };
            CARROT_TEST_REQUIRE(trigger != nullptr);
            CARROT_TEST_REQUIRE(trigger->trigger.has_value());
            CARROT_TEST_REQUIRE(trigger->collision.has_value());
            CARROT_TEST_REQUIRE(trigger->trigger->trigger_id == "inn_trigger_1");
            CARROT_TEST_REQUIRE(trigger->trigger->trigger_kind == "unlock_quest");
        }

        void test_trigger_query_reports_enter_stay_and_exit()
        {
            world::world_t world;

            world::world_object_t actor;
            actor.id = 999;
            actor.transform = world::transform_component_t{
                .position = { 0.f, 0.f },
                .scale = { 1.f, 1.f }
            };
            actor.collision = world::collision_component_t{
                .half_extents = { 0.3f, 0.2f },
                .offset = { 0.f, -0.2f }
            };

            world::world_object_t& trigger{ world.create_object() };
            trigger.name = "TestTrigger";
            trigger.type = "Trigger";
            trigger.transform = world::transform_component_t{
                .position = { 1.f, -1.f },
                .scale = { 1.f, 1.f }
            };
            trigger.collision = world::collision_component_t{
                .half_extents = { 0.5f, 1.f },
                .offset = { 0.5f, 1.f }
            };
            trigger.trigger = world::trigger_component_t{
                .trigger_id = "test.trigger",
                .trigger_kind = "test_kind"
            };

            std::unordered_set<world::world_object_id_t> active_trigger_ids;

            actor.transform->position = { 1.2f, 0.3f };
            const auto entered{ world::update_trigger_overlaps(actor, world, active_trigger_ids) };
            CARROT_TEST_REQUIRE(entered.entered.size() == 1u);
            CARROT_TEST_REQUIRE(entered.entered.front()->id == trigger.id);
            CARROT_TEST_REQUIRE(active_trigger_ids.contains(trigger.id));

            const auto staying{ world::update_trigger_overlaps(actor, world, active_trigger_ids) };
            CARROT_TEST_REQUIRE(staying.entered.empty());
            CARROT_TEST_REQUIRE(staying.staying.size() == 1u);
            CARROT_TEST_REQUIRE(staying.staying.front()->id == trigger.id);

            actor.transform->position = { 3.f, 0.3f };
            const auto exited{ world::update_trigger_overlaps(actor, world, active_trigger_ids) };
            CARROT_TEST_REQUIRE(exited.exited.size() == 1u);
            CARROT_TEST_REQUIRE(exited.exited.front()->id == trigger.id);
            CARROT_TEST_REQUIRE(active_trigger_ids.empty());
        }

        void test_trigger_monitor_emits_authored_enter_and_exit_events()
        {
            world::world_t world;

            world::world_object_t actor;
            actor.id = 999;
            actor.transform = world::transform_component_t{
                .position = { 0.f, 0.f },
                .scale = { 1.f, 1.f }
            };
            actor.collision = world::collision_component_t{
                .half_extents = { 0.3f, 0.2f },
                .offset = { 0.f, -0.2f }
            };

            world::world_object_t& trigger{ world.create_object() };
            trigger.name = "TestTrigger";
            trigger.type = "Trigger";
            trigger.transform = world::transform_component_t{
                .position = { 1.f, -1.f },
                .scale = { 1.f, 1.f }
            };
            trigger.collision = world::collision_component_t{
                .half_extents = { 0.5f, 1.f },
                .offset = { 0.5f, 1.f }
            };
            trigger.trigger = world::trigger_component_t{
                .trigger_id = "test.trigger",
                .trigger_kind = "test_kind"
            };

            world::trigger_monitor_t monitor;

            actor.transform->position = { 1.2f, 0.3f };
            monitor.update(actor, world);
            std::vector<world::trigger_event_t> events{ monitor.consume_pending_events() };
            CARROT_TEST_REQUIRE(events.size() == 1u);
            CARROT_TEST_REQUIRE(events.front().phase == world::trigger_event_phase_t::entered);
            CARROT_TEST_REQUIRE(events.front().object_id == trigger.id);
            CARROT_TEST_REQUIRE(events.front().trigger_id == "test.trigger");
            CARROT_TEST_REQUIRE(events.front().trigger_kind == "test_kind");

            monitor.update(actor, world);
            CARROT_TEST_REQUIRE(monitor.consume_pending_events().empty());

            actor.transform->position = { 3.f, 0.3f };
            monitor.update(actor, world);
            events = monitor.consume_pending_events();
            CARROT_TEST_REQUIRE(events.size() == 1u);
            CARROT_TEST_REQUIRE(events.front().phase == world::trigger_event_phase_t::exited);
            CARROT_TEST_REQUIRE(events.front().object_id == trigger.id);
        }
    } // namespace

        void register_scene_loading_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
        {
        tests.emplace_back("asset discovery finds scene manifests", test_asset_discovery_finds_scene_manifest);
        tests.emplace_back("asset discovery skips missing mount root without throwing",
                           test_asset_discovery_skips_missing_mount_root_without_throwing);
        tests.emplace_back("asset discovery skips file mount root without throwing",
                           test_asset_discovery_skips_file_mount_root_without_throwing);
        tests.emplace_back("tilemap world bridge imports authored objects", test_tilemap_world_bridge_imports_authored_objects);
        tests.emplace_back("tilemap world bridge imports tileset collision as static colliders",
                           test_tilemap_world_bridge_imports_tileset_collision_as_static_colliders);
        tests.emplace_back("tilemap world bridge imports object layer tile collision as static collider",
                           test_tilemap_world_bridge_imports_object_layer_tile_collision_as_static_collider);
        tests.emplace_back("tilemap world bridge imports authored triggers",
                           test_tilemap_world_bridge_imports_authored_triggers);
        tests.emplace_back("tilemap world bridge imports visibility regions",
                           test_tilemap_world_bridge_imports_visibility_regions);
        tests.emplace_back("tiled group visibility zone properties flow to child layers",
                           test_tiled_group_visibility_zone_properties_flow_to_child_layers);
        tests.emplace_back("tiled nested group properties flow to child layers",
                           test_tiled_nested_group_properties_flow_to_child_layers);
        tests.emplace_back("world layering uses explicit visibility zones",
                           test_world_layering_uses_explicit_visibility_zones);
        tests.emplace_back("trigger monitor emits authored enter and exit events",
                           test_trigger_monitor_emits_authored_enter_and_exit_events);
        tests.emplace_back("tiled layer front properties import from latest town map",
                           test_tiled_layer_front_properties_import_from_latest_town_map);
        tests.emplace_back("world layering resolves conditional and always front",
                           test_world_layering_resolves_conditional_and_always_front);
        tests.emplace_back("world layering does not force waterfall layers into background",
                           test_world_layering_does_not_force_waterfall_layers_into_background);
        tests.emplace_back("world layering debug snapshot round trips through world",
                           test_world_layering_debug_snapshot_round_trips_through_world);
        tests.emplace_back("tiled authored data validation reports visibility zone contract issues",
                           test_tiled_authored_data_validation_reports_visibility_zone_contract_issues);
        tests.emplace_back("imported sandbox town has no tiled authored data validation issues",
                           test_imported_sandbox_town_has_no_tiled_authored_data_validation_issues);
        tests.emplace_back("tiled point objects import as explicit point geometry",
                           test_tiled_point_objects_import_as_explicit_point_geometry);
        tests.emplace_back("tiled polygon geometry parses into object metadata",
                           test_tiled_polygon_geometry_parses_into_object_metadata);
        tests.emplace_back("typed object conventions parse current sandbox objects",
                           test_typed_object_conventions_parse_current_sandbox_objects);
        tests.emplace_back("tiled authored data validation reports typed object contract issues",
                           test_tiled_authored_data_validation_reports_typed_object_contract_issues);
        tests.emplace_back("tiled authored data validation reports light contract issues",
                           test_tiled_authored_data_validation_reports_light_contract_issues);
        tests.emplace_back("scene load options helper carries runtime bindings",
                           test_scene_load_options_helper_carries_runtime_bindings);
        tests.emplace_back("scene runtime default bindings apply to load requests",
                           test_scene_runtime_default_bindings_apply_to_load_requests);
        tests.emplace_back("scene runtime explicit load bindings override runtime defaults",
                           test_scene_runtime_explicit_load_bindings_override_runtime_defaults);
        tests.emplace_back("game view camera surface reads and writes camera state",
                           test_game_view_camera_surface_reads_and_writes_camera_state);
        tests.emplace_back("tiled tile animation metadata imports from sandbox town",
                           test_tiled_tile_animation_metadata_imports_from_sandbox_town);
        tests.emplace_back("tile animation resolves expected frame by elapsed time",
                           test_tile_animation_resolves_expected_frame_by_elapsed_time);
        tests.emplace_back("tiled tilemap import accepts unsupported features non-fatally",
                           test_tiled_tilemap_import_accepts_unsupported_features_non_fatally);
        tests.emplace_back("scene loader positive path", test_scene_loader_loads_scene_successfully);
        tests.emplace_back("scene loader loads sandbox town successfully", test_scene_loader_loads_sandbox_town_successfully);
        tests.emplace_back("scene loader assigns player collision config", test_scene_loader_assigns_player_collision_config);
        tests.emplace_back("scene loader imports authored lighting", test_scene_loader_imports_authored_lighting);
        tests.emplace_back("scene loader follow light tracks player from authored offset",
                           test_scene_loader_follow_light_tracks_player_from_authored_offset);
        tests.emplace_back("scene loader imports authored trigger component",
                           test_scene_loader_imports_authored_trigger_component);
        tests.emplace_back("scene loader missing scene failure path", test_scene_loader_fails_for_missing_scene);
        tests.emplace_back("scene loader supports spawn override", test_scene_loader_supports_spawn_override);
        tests.emplace_back("scene loader supports sandbox town spawn overrides",
                           test_scene_loader_supports_sandbox_town_spawn_overrides);
        tests.emplace_back("scene loader fails for missing spawn marker", test_scene_loader_fails_for_missing_spawn_marker);
        tests.emplace_back("scene loader preserves existing world on failed spawn override",
                           test_scene_loader_preserves_existing_world_on_failed_spawn_override);
        tests.emplace_back("scene asset importer parses camera modes", test_scene_asset_importer_parses_camera_modes);
        tests.emplace_back("scene asset importer rejects empty spawn marker", test_scene_asset_importer_rejects_empty_spawn_marker);
        tests.emplace_back("scene asset reference validation rejects missing tilemap", test_scene_asset_reference_validation_rejects_missing_tilemap);
        tests.emplace_back("scene asset reference validation rejects missing player sprite", test_scene_asset_reference_validation_rejects_missing_player_sprite);
        tests.emplace_back("scene asset reference validation rejects missing initial music", test_scene_asset_reference_validation_rejects_missing_initial_music);
        tests.emplace_back("door transition rejects unknown target scene", test_door_transition_request_rejects_unknown_target_scene);
        tests.emplace_back("door transition rejects unresolved legacy target map", test_door_transition_request_rejects_unresolved_legacy_target_map);
        tests.emplace_back("door transition rejects missing target marker", test_door_transition_request_rejects_missing_target_marker);
        tests.emplace_back("validate scene transition targets rejects invalid door in world", test_validate_scene_transition_targets_rejects_invalid_door_in_world);
        tests.emplace_back("validate scene transition targets rejects missing destination marker",
                           test_validate_scene_transition_targets_rejects_missing_destination_marker);
        tests.emplace_back("scene validation report does not realize destination tilemaps",
                           test_scene_validation_report_does_not_realize_destination_tilemaps);
        tests.emplace_back("sandbox scene transition requests connect all three scenes",
                           test_sandbox_scene_transition_requests_connect_all_three_scenes);
        tests.emplace_back("transition runtime state preserves opened container across scene reload",
                           test_transition_runtime_state_preserves_opened_container_across_scene_reload);
        tests.emplace_back("transition runtime state restores player facing after rebind",
                           test_transition_runtime_state_restores_player_facing_after_rebind);
        tests.emplace_back("scene continuity builds stable object keys from name and source",
                           test_scene_continuity_builds_stable_object_keys_from_name_and_source);
        tests.emplace_back("scene continuity flag store tracks named flags per scene object",
                           test_scene_continuity_flag_store_tracks_named_flags_per_scene_object);
        tests.emplace_back("scene continuity applies flagged callback to matching objects",
                           test_scene_continuity_applies_flagged_callback_to_matching_objects);
        tests.emplace_back("scene runtime snapshot defaults to idle state",
                           test_scene_runtime_snapshot_defaults_to_idle_state);
        tests.emplace_back("scene runtime state labels match expected diagnostics",
                           test_scene_runtime_state_labels_match_expected_diagnostics);
        tests.emplace_back("scene runtime summary defaults to empty world state",
                           test_scene_runtime_summary_defaults_to_empty_world_state);
        tests.emplace_back("scene runtime summary reports loaded scene world state",
                           test_scene_runtime_summary_reports_loaded_scene_world_state);
        tests.emplace_back("scene runtime object summaries report loaded scene objects",
                           test_scene_runtime_object_summaries_report_loaded_scene_objects);
        tests.emplace_back("scene runtime systems summary defaults to unbound state",
                           test_scene_runtime_systems_summary_defaults_to_unbound_state);
        tests.emplace_back("scene runtime systems summary reports bound runtime state",
                           test_scene_runtime_systems_summary_reports_bound_runtime_state);
        tests.emplace_back("transition presentation is hidden when runtime is idle",
                           test_transition_presentation_is_hidden_when_runtime_is_idle);
        tests.emplace_back("transition presentation exposes loading overlay during prepare",
                           test_transition_presentation_exposes_loading_overlay_during_prepare);
        tests.emplace_back("scene runtime uses black fade defaults for transition overlays",
                           test_transition_presentation_uses_black_fade_defaults);
        tests.emplace_back("scene runtime uses engine camera defaults",
                           test_scene_runtime_uses_engine_camera_defaults);
        tests.emplace_back("scene runtime project default camera resolves over engine default",
                           test_scene_runtime_project_default_camera_resolves_over_engine_default);
        tests.emplace_back("scene camera override preserves unspecified defaults",
                           test_scene_camera_override_preserves_unspecified_defaults);
        tests.emplace_back("scene runtime can disable project default transition overlay",
                           test_scene_runtime_can_disable_project_default_transition_overlay);
        tests.emplace_back("scene runtime project default resolves over engine default",
                           test_scene_runtime_project_default_resolves_over_engine_default);
        tests.emplace_back("transition overlay override none disables defaults",
                           test_transition_overlay_override_none_disables_defaults);
        tests.emplace_back("transition overlay override fade applies requested fields",
                           test_transition_overlay_override_fade_applies_requested_fields);
        tests.emplace_back("transition overlay override loading screen applies text configuration",
                           test_transition_overlay_override_loading_screen_applies_text_configuration);
        tests.emplace_back("transition overlay override wipe selects wipe style",
                           test_transition_overlay_override_wipe_selects_wipe_style);
        tests.emplace_back("transition effect override preserves named effect identity",
                           test_transition_effect_override_preserves_named_effect_identity);
        tests.emplace_back("renderer composite overlay routes to composite stage",
                           test_renderer_composite_overlay_routes_to_composite_stage);
        tests.emplace_back("renderer transition fade routes to composite stage",
                           test_renderer_transition_fade_routes_to_composite_stage);
        tests.emplace_back("renderer transition battle swirl routes to capture textured stage",
                           test_renderer_transition_battle_swirl_routes_to_capture_textured_stage);
        tests.emplace_back("renderer bloom routes to composite stage when enabled",
                           test_renderer_bloom_routes_to_composite_stage_when_enabled);
        tests.emplace_back("renderer bloom can be disabled",
                           test_renderer_bloom_can_be_disabled);
        tests.emplace_back("renderer composite and overlay debug use distinct stage spaces",
                           test_renderer_composite_and_overlay_debug_use_distinct_stage_spaces);
        tests.emplace_back("renderer ui and log console text use distinct presentation channels",
                           test_renderer_ui_and_log_console_text_use_distinct_presentation_channels);
        tests.emplace_back("renderer presentation window registration delegates to rhi",
                           test_renderer_presentation_window_registration_delegates_to_rhi);
        tests.emplace_back("renderer world light overflow is visible in stats",
                           test_renderer_world_light_overflow_is_visible_in_stats);
        tests.emplace_back("renderer extracts world render items before world execution",
                           test_renderer_extracts_world_render_items_before_world_execution);
        tests.emplace_back("loaded tilemap asset builds sparse render chunks for tile layers",
                           test_loaded_tilemap_asset_builds_sparse_render_chunks_for_tile_layers);
        tests.emplace_back("renderer dispatches world item cull compute for world items",
                           test_renderer_dispatches_world_item_cull_compute_for_world_items);
        tests.emplace_back("renderer records indirect world stage for world items",
                           test_renderer_records_indirect_world_stage_for_world_items);
        tests.emplace_back("interaction outcome dispatch routes scene transition and container",
                           test_interaction_outcome_dispatch_routes_scene_transition_and_container);
            tests.emplace_back("scene runtime rejects overlapping load requests",
                               test_scene_runtime_rejects_overlapping_load_requests);
            tests.emplace_back("scene runtime listener sees no current context on first load",
                               test_scene_runtime_listener_sees_no_current_context_on_first_load);
            tests.emplace_back("scene runtime listener sees previous context before transition and active context after",
                               test_scene_runtime_listener_sees_previous_context_before_transition_and_active_context_after);
            tests.emplace_back("scene runtime after listener sees post-activation runtime state",
                               test_scene_runtime_after_listener_sees_post_activation_runtime_state);
            tests.emplace_back("scene runtime rebuild current scene requires active scene",
                               test_scene_runtime_rebuild_current_scene_requires_active_scene);
            tests.emplace_back("game runtime debug toggles drive world overlay state",
                               test_game_runtime_debug_toggles_drive_world_overlay_state);
            tests.emplace_back("scene runtime can rebuild current scene",
                               test_scene_runtime_can_rebuild_current_scene);
            tests.emplace_back("scene runtime asset driven rebuild carries structural refresh context",
                               test_scene_runtime_asset_driven_rebuild_carries_structural_refresh_context);
            tests.emplace_back("scene runtime asset driven rebuild rejects non structural asset",
                               test_scene_runtime_asset_driven_rebuild_rejects_non_structural_asset);
            tests.emplace_back("scene runtime failed rebuild preserves active scene",
                               test_scene_runtime_failed_rebuild_preserves_active_scene);
            tests.emplace_back("incremental scene load task spreads work across multiple advances",
                               test_incremental_scene_load_task_spreads_work_across_multiple_advances);
        tests.emplace_back("incremental scene load task fails for missing spawn marker",
                           test_incremental_scene_load_task_fails_for_missing_spawn_marker);
        tests.emplace_back("prepared tilemap world data applies cleanly to world",
                           test_prepared_tilemap_world_data_applies_cleanly_to_world);
        tests.emplace_back("sandbox town player sprite exposes walk animation",
                           test_sandbox_town_player_sprite_exposes_walk_animation);
        tests.emplace_back("sandbox town player animator can play walk animation",
                           test_sandbox_town_player_animator_can_play_walk_animation);
        tests.emplace_back("player controller can switch to walk animation in sandbox town",
                           test_player_controller_can_switch_to_walk_animation_in_sandbox_town);
        tests.emplace_back("player controller blocks against imported sandbox town collision",
                           test_player_controller_blocks_against_imported_sandbox_town_collision);
        tests.emplace_back("player controller can move away after blocking against sandbox town collision",
                           test_player_controller_can_move_away_after_blocking_against_sandbox_town_collision);
        tests.emplace_back("player controller can escape small initial overlap in sandbox town collision",
                           test_player_controller_can_escape_small_initial_overlap_in_sandbox_town_collision);
        tests.emplace_back("player controller slides along vertical blocker",
                           test_player_controller_slides_along_vertical_blocker);
        tests.emplace_back("trigger query reports enter stay and exit",
                           test_trigger_query_reports_enter_stay_and_exit);
    }
} // namespace carrot::tests
