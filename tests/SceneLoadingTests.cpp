#include "TestCommon.h"

#include "Assets/AssetDiscovery.h"
#include "Assets/AssetManager.h"
#include "Assets/Audio/AudioAssetManifestImporter.h"
#include "Assets/Scene/SceneAssetManifestImporter.h"
#include "Assets/Sprite/SpriteAssetManifestImporter.h"
#include "Assets/Texture/TextureAssetManifestImporter.h"
#include "Assets/Tilemap/TilemapAssetManifestImporter.h"
#include "WorldInteractionHelpers.h"
#include "IO/VirtualFileSystem.h"
#include "RHI/CommandQueue.h"
#include "RHI/RHI.h"
#include "Utils/JSON/Public/JsonDocument.h"
#include "World/Import/TilemapWorldBridge.h"
#include "World/SceneLoader.h"
#include "World/World.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        class fake_texture_t final : public rhi::rhi_texture_t
        {
        public:
            explicit fake_texture_t(const rhi::texture_create_info_t& info) noexcept
                : _width{ info.width }, _height{ info.height }, _format{ info.format } {}

            [[nodiscard]] uint32_t width() const noexcept override { return _width; }
            [[nodiscard]] uint32_t height() const noexcept override { return _height; }
            [[nodiscard]] rhi::texture_format_t format() const noexcept override { return _format; }

        private:
            uint32_t _width{ 0 };
            uint32_t _height{ 0 };
            rhi::texture_format_t _format{ rhi::texture_format_t::rgba8_srgb };
        };

        class fake_buffer_t final : public rhi::rhi_buffer_t
        {
        public:
            explicit fake_buffer_t(const rhi::buffer_create_info_t& info) noexcept
                : rhi::rhi_buffer_t{ info.size_bytes, info.usage }, _storage(info.size_bytes) {}

            [[nodiscard]] bool write(const void* data, const size_t size_bytes, const size_t offset_bytes = 0) override
            {
                if (!data || offset_bytes + size_bytes > _storage.size())
                    return false;

                std::memcpy(_storage.data() + offset_bytes, data, size_bytes);
                return true;
            }

        private:
            std::vector<std::byte> _storage;
        };

        class fake_sampler_t final : public rhi::rhi_sampler_t
        {
        public:
            explicit fake_sampler_t(const rhi::sampler_desc_t& desc) noexcept
                : rhi::rhi_sampler_t{ desc } {}
        };

        class fake_command_queue_t final : public rhi::rhi_command_queue_t
        {
        public:
            void submit([[maybe_unused]] rhi::rhi_command_list_t* cmd_list,
                        [[maybe_unused]] rhi::rhi_fence_t* fence_to_signal = nullptr,
                        [[maybe_unused]] rhi::rhi_semaphore_t* wait_semaphore = nullptr,
                        [[maybe_unused]] rhi::rhi_semaphore_t* signal_semaphore = nullptr) override {}

            void wait_idle() override {}
        };

        class fake_context_t final : public rhi::rhi_context_t
        {
        public:
            void begin_frame() override {}
            void record_frame() override {}
            void end_frame() override {}
            void release_asset_references() override {}
            void resize([[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height) override {}

            [[nodiscard]] rhi::rhi_device_t* get_device() const noexcept override { return nullptr; }
            [[nodiscard]] rhi::rhi_swapchain_t* get_swapchain() const noexcept override { return nullptr; }
            [[nodiscard]] rhi::rhi_command_queue_t* get_command_queue() const noexcept override
            {
                return const_cast<fake_command_queue_t*>(&_queue);
            }

            [[nodiscard]] rhi::graphics_api get_graphics_api() const noexcept override
            {
                return rhi::graphics_api::default_api;
            }

            [[nodiscard]] std::unique_ptr<rhi::rhi_texture_t> create_texture_2d(const rhi::texture_create_info_t& info) override
            {
                return std::make_unique<fake_texture_t>(info);
            }

            [[nodiscard]] std::unique_ptr<rhi::rhi_buffer_t> create_buffer(const rhi::buffer_create_info_t& info) override
            {
                return std::make_unique<fake_buffer_t>(info);
            }

            [[nodiscard]] std::unique_ptr<rhi::rhi_sampler_t> create_sampler(const rhi::sampler_desc_t& desc) const override
            {
                return std::make_unique<fake_sampler_t>(desc);
            }

            void set_textured_quad_geometry([[maybe_unused]] const rhi::rhi_buffer_t& vertex_buffer,
                                            [[maybe_unused]] const rhi::rhi_buffer_t& index_buffer) override {}

            void set_textured_quad_batches([[maybe_unused]] std::span<const renderer::textured_quad_batch_t> batches) override {}
            void set_textured_quad_view_projection([[maybe_unused]] const chlm::float4x4& view_projection) override {}
            void set_textured_quad_viewport([[maybe_unused]] const rhi::render_viewport_t& viewport) override {}

            [[nodiscard]] rhi::rhi_sampler_t* get_or_create_sampler(const rhi::sampler_desc_t& desc) override
            {
                const auto [it, inserted]{
                    _samplers.emplace(desc, std::make_unique<fake_sampler_t>(desc))
                };
                return it->second.get();
            }

            void bind_textured_quad_resources([[maybe_unused]] const rhi::rhi_texture_t& texture,
                                              [[maybe_unused]] const rhi::rhi_sampler_t& sampler) override {}

            void wait_idle() override {}

        private:
            fake_command_queue_t _queue;
            std::unordered_map<rhi::sampler_desc_t, std::unique_ptr<fake_sampler_t>, rhi::sampler_desc_hash_t> _samplers;
        };

        [[nodiscard]] std::filesystem::path game_assets_root()
        {
            return std::filesystem::path{ CARROT_SOURCE_ROOT } / "src" / "Game" / "assets";
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
            CARROT_TEST_REQUIRE(world.find_object_by_name("NorthDoor") != nullptr);
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

            CARROT_TEST_REQUIRE(!sandbox::make_scene_transition_request(assets, door).has_value());
            CARROT_TEST_REQUIRE(!sandbox::validate_scene_transition_target(assets, door));
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

            CARROT_TEST_REQUIRE(!sandbox::make_scene_transition_request(assets, door).has_value());
            CARROT_TEST_REQUIRE(!sandbox::validate_scene_transition_target(assets, door));
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

            CARROT_TEST_REQUIRE(!sandbox::make_scene_transition_request(assets, door).has_value());
            CARROT_TEST_REQUIRE(!sandbox::validate_scene_transition_target(assets, door));
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

            CARROT_TEST_REQUIRE(!sandbox::validate_scene_transition_targets(assets, world));
        }
    } // namespace

    void register_scene_loading_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("asset discovery finds scene manifests", test_asset_discovery_finds_scene_manifest);
        tests.emplace_back("tilemap world bridge imports authored objects", test_tilemap_world_bridge_imports_authored_objects);
        tests.emplace_back("scene loader positive path", test_scene_loader_loads_scene_successfully);
        tests.emplace_back("scene loader missing scene failure path", test_scene_loader_fails_for_missing_scene);
        tests.emplace_back("scene loader supports spawn override", test_scene_loader_supports_spawn_override);
        tests.emplace_back("scene loader fails for missing spawn marker", test_scene_loader_fails_for_missing_spawn_marker);
        tests.emplace_back("scene asset importer parses camera modes", test_scene_asset_importer_parses_camera_modes);
        tests.emplace_back("scene asset importer rejects empty spawn marker", test_scene_asset_importer_rejects_empty_spawn_marker);
        tests.emplace_back("scene asset reference validation rejects missing tilemap", test_scene_asset_reference_validation_rejects_missing_tilemap);
        tests.emplace_back("scene asset reference validation rejects missing player sprite", test_scene_asset_reference_validation_rejects_missing_player_sprite);
        tests.emplace_back("scene asset reference validation rejects missing initial music", test_scene_asset_reference_validation_rejects_missing_initial_music);
        tests.emplace_back("door transition rejects unknown target scene", test_door_transition_request_rejects_unknown_target_scene);
        tests.emplace_back("door transition rejects unresolved legacy target map", test_door_transition_request_rejects_unresolved_legacy_target_map);
        tests.emplace_back("door transition rejects missing target marker", test_door_transition_request_rejects_missing_target_marker);
        tests.emplace_back("validate scene transition targets rejects invalid door in world", test_validate_scene_transition_targets_rejects_invalid_door_in_world);
    }
} // namespace carrot::tests
