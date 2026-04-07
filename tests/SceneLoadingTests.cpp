//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "Assets/AssetDiscovery.h"
#include "Assets/AssetManager.h"
#include "Assets/Audio/AudioAssetManifestImporter.h"
#include "Assets/Scene/SceneAssetManifestImporter.h"
#include "Assets/Sprite/SpriteAssetManifestImporter.h"
#include "Assets/Texture/TextureAssetManifestImporter.h"
#include "Assets/Tilemap/TiledTilemapAssetImporter.h"
#include "Assets/Tilemap/TilemapAssetManifestImporter.h"
#include "Assets/Tilemap/TypedObjectConventions.h"
#include "Assets/Tilemap/TilemapValidation.h"
#include "Core/GameView.h"
#include "Renderer/Renderer.h"
#include "TransitionRuntimeState.h"
#include "WorldInteractionHelpers.h"
#include "IO/VirtualFileSystem.h"
#include "RHI/CommandQueue.h"
#include "RHI/RHI.h"
#include "Utils/JSON/Public/JsonDocument.h"
#include "World/Controllers/PlayerController.h"
#include "World/Import/TilemapWorldBridge.h"
#include "World/SceneLoader.h"
#include "World/World.h"
#include "World/WorldLayering.h"

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
            void record_textured_quad_stage([[maybe_unused]] const rhi::textured_quad_stage_record_t& stage) override {}
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
            CARROT_TEST_REQUIRE(sign != nullptr);
            CARROT_TEST_REQUIRE(chest != nullptr);
            CARROT_TEST_REQUIRE(door != nullptr);
            CARROT_TEST_REQUIRE(trigger != nullptr);
            CARROT_TEST_REQUIRE(visibility_zone != nullptr);

            const auto typed_sign{ assets::as_typed_sign(*sign) };
            const auto typed_container{ assets::as_typed_container(*chest) };
            const auto typed_door{ assets::as_typed_door(*door) };
            const auto typed_trigger{ assets::as_typed_trigger(*trigger) };
            const auto typed_visibility_zone{ assets::as_typed_visibility_zone(*visibility_zone) };
            CARROT_TEST_REQUIRE(typed_sign.has_value());
            CARROT_TEST_REQUIRE(typed_container.has_value());
            CARROT_TEST_REQUIRE(typed_door.has_value());
            CARROT_TEST_REQUIRE(typed_trigger.has_value());
            CARROT_TEST_REQUIRE(typed_visibility_zone.has_value());
            CARROT_TEST_REQUIRE(typed_sign->message_id == "sign.welcome");
            CARROT_TEST_REQUIRE(typed_container->loot_table == "starter_chest");
            CARROT_TEST_REQUIRE(typed_door->target_scene == "scene.sandbox.inn");
            CARROT_TEST_REQUIRE(typed_door->target_marker == "EntryFromTown");
            CARROT_TEST_REQUIRE(typed_trigger->trigger_id == "inn_trigger_1");
            CARROT_TEST_REQUIRE(typed_trigger->trigger_kind == "unlock_quest");
            CARROT_TEST_REQUIRE(typed_visibility_zone->visibility_zone_id == "inn_roof");
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

            CARROT_TEST_REQUIRE(!sandbox::validate_scene_transition_targets(assets, world));
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

            const std::optional<sandbox::scene_transition_request_t> inn_request{
                sandbox::make_scene_transition_request(assets, *town_to_inn)
            };
            const std::optional<sandbox::scene_transition_request_t> item_shop_request{
                sandbox::make_scene_transition_request(assets, *town_to_item_shop)
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
            const std::optional<sandbox::scene_transition_request_t> inn_exit_request{
                sandbox::make_scene_transition_request(assets, *inn_exit_door)
            };
            CARROT_TEST_REQUIRE(inn_exit_request.has_value());
            CARROT_TEST_REQUIRE(inn_exit_request->scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(inn_exit_request->marker_name == "InnExteriorSpawn");

            world::world_t item_shop_world;
            CARROT_TEST_REQUIRE(world::scene_loader_t::load_scene(item_shop_world, assets, item_shop_request->scene_id, item_shop_request->marker_name));
            const world::world_object_t* item_shop_exit_door{ item_shop_world.find_object_by_name("DoorToTownFromItemShop") };
            CARROT_TEST_REQUIRE(item_shop_exit_door != nullptr);
            const std::optional<sandbox::scene_transition_request_t> item_shop_exit_request{
                sandbox::make_scene_transition_request(assets, *item_shop_exit_door)
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
    } // namespace

    void register_scene_loading_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("asset discovery finds scene manifests", test_asset_discovery_finds_scene_manifest);
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
        tests.emplace_back("tiled tile animation metadata imports from sandbox town",
                           test_tiled_tile_animation_metadata_imports_from_sandbox_town);
        tests.emplace_back("tile animation resolves expected frame by elapsed time",
                           test_tile_animation_resolves_expected_frame_by_elapsed_time);
        tests.emplace_back("tiled tilemap import accepts unsupported features non-fatally",
                           test_tiled_tilemap_import_accepts_unsupported_features_non_fatally);
        tests.emplace_back("scene loader positive path", test_scene_loader_loads_scene_successfully);
        tests.emplace_back("scene loader loads sandbox town successfully", test_scene_loader_loads_sandbox_town_successfully);
        tests.emplace_back("scene loader assigns player collision config", test_scene_loader_assigns_player_collision_config);
        tests.emplace_back("scene loader imports authored trigger component",
                           test_scene_loader_imports_authored_trigger_component);
        tests.emplace_back("scene loader missing scene failure path", test_scene_loader_fails_for_missing_scene);
        tests.emplace_back("scene loader supports spawn override", test_scene_loader_supports_spawn_override);
        tests.emplace_back("scene loader supports sandbox town spawn overrides",
                           test_scene_loader_supports_sandbox_town_spawn_overrides);
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
        tests.emplace_back("validate scene transition targets rejects missing destination marker",
                           test_validate_scene_transition_targets_rejects_missing_destination_marker);
        tests.emplace_back("sandbox scene transition requests connect all three scenes",
                           test_sandbox_scene_transition_requests_connect_all_three_scenes);
        tests.emplace_back("transition runtime state preserves opened container across scene reload",
                           test_transition_runtime_state_preserves_opened_container_across_scene_reload);
        tests.emplace_back("transition runtime state restores player facing after rebind",
                           test_transition_runtime_state_restores_player_facing_after_rebind);
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
