//
// Created by Zack Shrout on 4/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "Assets/Tilemap/TiledWorld.h"
#include "IO/VirtualFileSystem.h"
#include "Utils/JSON/Public/JsonDocument.h"
#include "World/WorldComposition.h"

#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        [[nodiscard]] std::filesystem::path workspace_root()
        {
            return std::filesystem::path{ CARROT_SOURCE_ROOT };
        }

        [[nodiscard]] std::filesystem::path game_assets_root()
        {
            return workspace_root() / "src/Sandbox/assets";
        }

        void mount_game(io::virtual_file_system_t& vfs)
        {
            vfs.mount("game", game_assets_root(), true);
        }

        [[nodiscard]] utils::json::json_document_t parse_json_file(const io::virtual_file_system_t& vfs,
                                                                   const std::string_view uri)
        {
            const auto native_path{ vfs.resolve_native_path(uri) };
            CARROT_TEST_REQUIRE(native_path.has_value());

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_file(native_path->string().c_str()));
            return doc;
        }

        void test_runtime_world_layout_builds_from_real_fixture()
        {
            io::virtual_file_system_t vfs;
            mount_game(vfs);

            constexpr std::string_view world_uri{ "game://tilemaps/worlds/MainOverworld.world" };
            const utils::json::json_document_t doc{ parse_json_file(vfs, world_uri) };
            const auto authored_world{ assets::parse_tiled_world(doc, world_uri, &vfs) };
            CARROT_TEST_REQUIRE(authored_world.has_value());

            const world::runtime_world_layout_t layout{
                world::runtime_world_layout_t::from_authored_world(*authored_world)
            };

            CARROT_TEST_REQUIRE(layout.chunks().size() == 42u);

            const world::runtime_world_chunk_descriptor_t& first_chunk{ layout.chunks().front() };
            CARROT_TEST_REQUIRE(first_chunk.identity.world_source_uri == world_uri);
            CARROT_TEST_REQUIRE(first_chunk.identity.map_source_uri == "game://tilemaps/worlds/MainMap1.json");
            CARROT_TEST_REQUIRE(first_chunk.identity.authored_chunk_index == 0u);
            CARROT_TEST_REQUIRE(first_chunk.map_file_name == "MainMap1.json");
            CARROT_TEST_REQUIRE(first_chunk.authored_bounds_px.x == 0);
            CARROT_TEST_REQUIRE(first_chunk.authored_bounds_px.y == 0);
            CARROT_TEST_REQUIRE(first_chunk.authored_bounds_px.width == 1600);
            CARROT_TEST_REQUIRE(first_chunk.authored_bounds_px.height == 1600);
            CARROT_TEST_REQUIRE(first_chunk.identity.stable_id() ==
                                "game://tilemaps/worlds/MainOverworld.world#0@game://tilemaps/worlds/MainMap1.json");

            const auto origin_matches{ layout.query_overlapping(world::authored_world_bounds_t{
                .x = 100,
                .y = 100,
                .width = 50,
                .height = 50
            }) };
            CARROT_TEST_REQUIRE(origin_matches.size() == 1u);
            CARROT_TEST_REQUIRE(origin_matches.front()->map_file_name == "MainMap1.json");

            const auto seam_matches{ layout.query_overlapping(world::authored_world_bounds_t{
                .x = 1590,
                .y = 100,
                .width = 40,
                .height = 50
            }) };
            CARROT_TEST_REQUIRE(seam_matches.size() == 2u);

            const auto chunk_states{ layout.instantiate_chunk_states() };
            CARROT_TEST_REQUIRE(chunk_states.size() == 42u);
            CARROT_TEST_REQUIRE(chunk_states.front().residency == world::runtime_world_chunk_residency_t::unloaded);
            CARROT_TEST_REQUIRE(chunk_states.front().tilemap_visuals_imported == false);
            CARROT_TEST_REQUIRE(chunk_states.front().collision_imported == false);
            CARROT_TEST_REQUIRE(chunk_states.front().objects_imported == false);
        }

        void test_runtime_world_layout_keeps_runtime_identity_distinct_from_authored_map_reference()
        {
            constexpr std::string_view json{
                R"({
                    "type": "world",
                    "maps": [
                        { "fileName": "shared_chunk.tmj", "x": 0, "y": 0, "width": 32, "height": 32 },
                        { "fileName": "shared_chunk.tmj", "x": 32, "y": 0, "width": 32, "height": 32 }
                    ]
                })"
            };

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_memory(json.data(), json.size()));

            const auto authored_world{ assets::parse_tiled_world(doc, "game://tilemaps/worlds/synthetic.world", nullptr) };
            CARROT_TEST_REQUIRE(authored_world.has_value());

            const world::runtime_world_layout_t layout{
                world::runtime_world_layout_t::from_authored_world(*authored_world)
            };
            CARROT_TEST_REQUIRE(layout.chunks().size() == 2u);

            const auto& first{ layout.chunks()[0] };
            const auto& second{ layout.chunks()[1] };
            CARROT_TEST_REQUIRE(first.identity.map_source_uri == second.identity.map_source_uri);
            CARROT_TEST_REQUIRE(first.identity.authored_chunk_index == 0u);
            CARROT_TEST_REQUIRE(second.identity.authored_chunk_index == 1u);
            CARROT_TEST_REQUIRE(first.identity.stable_id() != second.identity.stable_id());
        }
    } // namespace

    void register_world_composition_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("runtime world layout builds from real fixture",
                           test_runtime_world_layout_builds_from_real_fixture);
        tests.emplace_back("runtime world layout keeps runtime identity distinct from authored map reference",
                           test_runtime_world_layout_keeps_runtime_identity_distinct_from_authored_map_reference);
    }
} // namespace carrot::tests
