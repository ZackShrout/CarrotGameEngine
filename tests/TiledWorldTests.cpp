//
// Created by Zack Shrout on 4/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "Assets/Tilemap/TiledWorld.h"
#include "IO/VirtualFileSystem.h"
#include "Utils/JSON/Public/JsonDocument.h"

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

        [[nodiscard]] std::filesystem::path engine_assets_root()
        {
            return workspace_root() / "src/Engine/assets";
        }

        [[nodiscard]] std::filesystem::path game_assets_root()
        {
            return workspace_root() / "src/Sandbox/assets";
        }

        void mount_all(io::virtual_file_system_t& vfs)
        {
            vfs.mount("engine", engine_assets_root(), true);
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

        void test_parse_real_tiled_world_fixture()
        {
            io::virtual_file_system_t vfs;
            mount_all(vfs);

            constexpr std::string_view world_uri{ "game://tilemaps/worlds/MainOverworld.world" };
            utils::json::json_document_t doc{ parse_json_file(vfs, world_uri) };
            const auto world_opt{ assets::parse_tiled_world(doc, world_uri, &vfs) };
            CARROT_TEST_REQUIRE(world_opt.has_value());

            const assets::tiled_world_t& world{ *world_opt };
            CARROT_TEST_REQUIRE(world.type == "world");
            CARROT_TEST_REQUIRE(world.only_show_adjacent_maps == false);
            CARROT_TEST_REQUIRE(world.source_uri == world_uri);
            CARROT_TEST_REQUIRE(world.maps.size() == 42u);

            const assets::tiled_world_map_entry_t& first_map{ world.maps.front() };
            CARROT_TEST_REQUIRE(first_map.file_name == "MainMap1.json");
            CARROT_TEST_REQUIRE(first_map.source_uri == "game://tilemaps/worlds/MainMap1.json");
            CARROT_TEST_REQUIRE(first_map.x == 0);
            CARROT_TEST_REQUIRE(first_map.y == 0);
            CARROT_TEST_REQUIRE(first_map.width == 1600);
            CARROT_TEST_REQUIRE(first_map.height == 1600);

            size_t missing_reference_warning_count{ 0u };
            for (const assets::tilemap_validation_issue_t& issue : world.validation_issues)
            {
                if (issue.code == "tiled.world.map_reference_missing")
                    ++missing_reference_warning_count;
            }

            CARROT_TEST_REQUIRE(missing_reference_warning_count == 42u);
        }

        void test_world_validation_warns_for_duplicate_and_overlapping_maps()
        {
            constexpr std::string_view json{
                R"({
                    "type": "world",
                    "maps": [
                        { "fileName": "chunk_a.tmj", "x": 0, "y": 0, "width": 100, "height": 100 },
                        { "fileName": "chunk_a.tmj", "x": 50, "y": 0, "width": 100, "height": 100 },
                        { "fileName": "chunk_b.tmj", "x": 10, "y": 10, "width": 0, "height": 32 },
                        { "x": 0, "y": 0, "width": 100, "height": 100 }
                    ]
                })"
            };

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_memory(json.data(), json.size()));

            const auto world_opt{ assets::parse_tiled_world(doc, "game://tilemaps/worlds/synthetic.world", nullptr) };
            CARROT_TEST_REQUIRE(world_opt.has_value());

            const assets::tiled_world_t& world{ *world_opt };
            CARROT_TEST_REQUIRE(world.maps.size() == 3u);

            bool saw_duplicate_warning{ false };
            bool saw_overlap_warning{ false };
            bool saw_non_positive_extent_warning{ false };
            bool saw_missing_file_name_error{ false };

            for (const assets::tilemap_validation_issue_t& issue : world.validation_issues)
            {
                saw_duplicate_warning = saw_duplicate_warning || issue.code == "tiled.world.map.duplicate_reference";
                saw_overlap_warning = saw_overlap_warning || issue.code == "tiled.world.map_bounds_overlap";
                saw_non_positive_extent_warning = saw_non_positive_extent_warning || issue.code == "tiled.world.map.non_positive_extent";
                saw_missing_file_name_error = saw_missing_file_name_error || issue.code == "tiled.world.map.missing_file_name";
            }

            CARROT_TEST_REQUIRE(saw_duplicate_warning);
            CARROT_TEST_REQUIRE(saw_overlap_warning);
            CARROT_TEST_REQUIRE(saw_non_positive_extent_warning);
            CARROT_TEST_REQUIRE(saw_missing_file_name_error);
        }
    } // namespace

    void register_tiled_world_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("parse real tiled world fixture", test_parse_real_tiled_world_fixture);
        tests.emplace_back("world validation warns for duplicate and overlapping maps",
                           test_world_validation_warns_for_duplicate_and_overlapping_maps);
    }
} // namespace carrot::tests
