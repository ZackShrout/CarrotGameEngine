//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "Assets/AssetManager.h"
#include "Assets/Audio/AudioAssetManifestImporter.h"
#include "Assets/Font/FontAssetManifestImporter.h"
#include "Assets/Scene/SceneAssetManifestImporter.h"
#include "Assets/Sprite/SpriteAssetManifestImporter.h"
#include "Assets/Texture/TextureAssetManifestImporter.h"
#include "Assets/Tilemap/TilemapAssetManifestImporter.h"
#include "IO/VirtualFileSystem.h"
#include "RHI/RHI.h"
#include "Utils/JSON/Public/JsonDocument.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <string_view>
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

        [[nodiscard]] std::filesystem::path engine_assets_root()
        {
            return std::filesystem::path{ CARROT_SOURCE_ROOT } / "assets";
        }

        [[nodiscard]] std::filesystem::path game_assets_root()
        {
            return std::filesystem::path{ CARROT_SOURCE_ROOT } / "src" / "Sandbox" / "assets";
        }

        [[nodiscard]] std::filesystem::path temp_root()
        {
            return std::filesystem::temp_directory_path() / "carrot_asset_iteration_tests";
        }

        [[nodiscard]] std::filesystem::path auto_reload_root()
        {
            return std::filesystem::temp_directory_path() / "carrot_asset_iteration_auto_reload_tests";
        }

        void reset_temp_root()
        {
            std::filesystem::remove_all(temp_root());
            std::filesystem::create_directories(temp_root() / "save");
        }

        void reset_auto_reload_root()
        {
            std::filesystem::remove_all(auto_reload_root());
            std::filesystem::create_directories(auto_reload_root() / "game" / "textures");
            std::filesystem::create_directories(auto_reload_root() / "save");
        }

        void mount_all(io::virtual_file_system_t& vfs)
        {
            vfs.mount("engine", engine_assets_root(), true);
            vfs.mount("game", game_assets_root(), true);
            vfs.mount("save", temp_root() / "save", false);
        }

        void mount_auto_reload_roots(io::virtual_file_system_t& vfs)
        {
            vfs.mount("game", auto_reload_root() / "game", true);
            vfs.mount("save", auto_reload_root() / "save", false);
        }

        [[nodiscard]] utils::json::json_document_t parse_json(std::string_view manifest_uri,
                                                              const io::virtual_file_system_t& vfs)
        {
            const auto native_path{ vfs.resolve_native_path(manifest_uri) };
            CARROT_TEST_REQUIRE(native_path.has_value());

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_file(native_path->string().c_str()));
            return doc;
        }

        void register_runtime_iteration_assets(assets::asset_manager_t& asset_manager, const io::virtual_file_system_t& vfs)
        {
            {
                auto doc{ parse_json("engine://textures/carrot_engine_logo_512.texture.json", vfs) };
                CARROT_TEST_REQUIRE(assets::texture_asset_manifest_importer_t::import(doc, asset_manager.textures().registry(), vfs,
                                                                                      "engine://textures/carrot_engine_logo_512.texture.json"));
            }
            {
                auto doc{ parse_json("game://textures/vraden.texture.json", vfs) };
                CARROT_TEST_REQUIRE(assets::texture_asset_manifest_importer_t::import(doc, asset_manager.textures().registry(), vfs,
                                                                                      "game://textures/vraden.texture.json"));
            }
            {
                auto doc{ parse_json("game://sprites/vraden.sprite.json", vfs) };
                CARROT_TEST_REQUIRE(assets::sprite_asset_manifest_importer_t::import(doc, asset_manager.sprites().registry(), vfs,
                                                                                     "game://sprites/vraden.sprite.json"));
            }
            {
                auto doc{ parse_json("engine://audio/victory.audio.json", vfs) };
                CARROT_TEST_REQUIRE(assets::audio_asset_manifest_importer_t::import(doc, asset_manager.audio().registry(), vfs,
                                                                                    "engine://audio/victory.audio.json"));
            }
            {
                auto doc{ parse_json("engine://fonts/roboto_regular.font.json", vfs) };
                CARROT_TEST_REQUIRE(assets::font_asset_manifest_importer_t::import(doc, asset_manager.fonts().registry(), vfs,
                                                                                   "engine://fonts/roboto_regular.font.json"));
            }
            {
                auto doc{ parse_json("game://tilemaps/test_overworld.tilemap.json", vfs) };
                CARROT_TEST_REQUIRE(assets::tilemap_asset_manifest_importer_t::import(doc, asset_manager.tilemaps().registry(), vfs,
                                                                                      "game://tilemaps/test_overworld.tilemap.json"));
            }
            {
                auto doc{ parse_json("game://scenes/test_overworld.scene.json", vfs) };
                CARROT_TEST_REQUIRE(assets::scene_asset_manifest_importer_t::import(doc, asset_manager.scenes().registry(), vfs,
                                                                                    "game://scenes/test_overworld.scene.json"));
            }
        }

        void test_runtime_iteration_statuses_expose_registered_assets()
        {
            reset_temp_root();

            io::virtual_file_system_t vfs;
            mount_all(vfs);
            auto rhi{ make_null_rhi() };
            CARROT_TEST_REQUIRE(rhi != nullptr);
            assets::asset_manager_t asset_manager{ vfs, *rhi };
            register_runtime_iteration_assets(asset_manager, vfs);

            const auto statuses{ asset_manager.collect_runtime_iteration_statuses() };
            CARROT_TEST_REQUIRE(statuses.size() == 7u);

            const auto texture_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::texture,
                                                                                   assets::make_asset_id("engine.carrot_engine_logo_512")) };
            CARROT_TEST_REQUIRE(texture_status.has_value());
            CARROT_TEST_REQUIRE(texture_status->dependency_shape == assets::asset_dependency_shape_t::leaf_runtime_data);
            CARROT_TEST_REQUIRE(texture_status->watch_mode == assets::asset_iteration_watch_mode_t::source_and_manifest_timestamps);
            CARROT_TEST_REQUIRE(texture_status->reload_policy == assets::asset_reload_policy_t::reloadable_live);
            CARROT_TEST_REQUIRE(!texture_status->has_last_attempt);

            const auto sprite_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::sprite,
                                                                                  assets::make_asset_id("sprite.vraden")) };
            CARROT_TEST_REQUIRE(sprite_status.has_value());
            CARROT_TEST_REQUIRE(sprite_status->dependency_shape == assets::asset_dependency_shape_t::referenced_runtime_assets);
            CARROT_TEST_REQUIRE(sprite_status->watch_mode == assets::asset_iteration_watch_mode_t::source_and_manifest_timestamps);
            CARROT_TEST_REQUIRE(sprite_status->dependency_summary.find("texture asset 'texture.vraden_sprite'") != std::string::npos);

            const auto audio_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::audio,
                                                                                 assets::make_asset_id("music.victory")) };
            CARROT_TEST_REQUIRE(audio_status.has_value());
            CARROT_TEST_REQUIRE(audio_status->dependency_shape == assets::asset_dependency_shape_t::leaf_runtime_data);
            CARROT_TEST_REQUIRE(audio_status->watch_mode == assets::asset_iteration_watch_mode_t::source_and_manifest_timestamps);
            CARROT_TEST_REQUIRE(audio_status->reload_policy == assets::asset_reload_policy_t::manual_refresh_only);

            const auto font_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::font,
                                                                                assets::make_asset_id("font.engine.roboto_regular")) };
            CARROT_TEST_REQUIRE(font_status.has_value());
            CARROT_TEST_REQUIRE(font_status->dependency_shape == assets::asset_dependency_shape_t::layout_or_presentation_contract);
            CARROT_TEST_REQUIRE(font_status->watch_mode == assets::asset_iteration_watch_mode_t::not_polled);
            CARROT_TEST_REQUIRE(font_status->reload_policy == assets::asset_reload_policy_t::restart_or_scene_rebuild_required);

            const auto tilemap_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::tilemap,
                                                                                   assets::make_asset_id("tilemap.test.overworld")) };
            CARROT_TEST_REQUIRE(tilemap_status.has_value());
            CARROT_TEST_REQUIRE(tilemap_status->dependency_shape == assets::asset_dependency_shape_t::scene_or_world_structure);
            CARROT_TEST_REQUIRE(tilemap_status->watch_mode == assets::asset_iteration_watch_mode_t::not_polled);
            CARROT_TEST_REQUIRE(tilemap_status->reload_policy == assets::asset_reload_policy_t::restart_or_scene_rebuild_required);

            const auto scene_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::scene,
                                                                                 assets::make_asset_id("scene.test.overworld")) };
            CARROT_TEST_REQUIRE(scene_status.has_value());
            CARROT_TEST_REQUIRE(scene_status->dependency_shape == assets::asset_dependency_shape_t::scene_or_world_structure);
            CARROT_TEST_REQUIRE(scene_status->watch_mode == assets::asset_iteration_watch_mode_t::not_polled);
            CARROT_TEST_REQUIRE(scene_status->manifest_uri.empty());
            CARROT_TEST_REQUIRE(scene_status->reload_policy == assets::asset_reload_policy_t::restart_or_scene_rebuild_required);
        }

        void test_manual_reload_records_last_attempt_details()
        {
            reset_temp_root();

            io::virtual_file_system_t vfs;
            mount_all(vfs);
            auto rhi{ make_null_rhi() };
            CARROT_TEST_REQUIRE(rhi != nullptr);
            assets::asset_manager_t asset_manager{ vfs, *rhi };
            register_runtime_iteration_assets(asset_manager, vfs);

            CARROT_TEST_REQUIRE(asset_manager.reload_asset(assets::asset_kind_t::texture, "engine.carrot_engine_logo_512"));
            CARROT_TEST_REQUIRE(asset_manager.reload_asset(assets::asset_kind_t::sprite, "sprite.vraden"));
            CARROT_TEST_REQUIRE(asset_manager.reload_asset(assets::asset_kind_t::audio, "music.victory"));

            const auto texture_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::texture,
                                                                                   assets::make_asset_id("engine.carrot_engine_logo_512")) };
            CARROT_TEST_REQUIRE(texture_status.has_value());
            CARROT_TEST_REQUIRE(texture_status->has_last_attempt);
            CARROT_TEST_REQUIRE(texture_status->last_result == assets::asset_iteration_result_t::success);
            CARROT_TEST_REQUIRE(texture_status->loaded_in_runtime_cache);
            CARROT_TEST_REQUIRE(texture_status->last_load_origin == assets::asset_load_origin_t::regenerated_from_source);
            CARROT_TEST_REQUIRE(texture_status->last_refresh_request_origin ==
                                assets::asset_iteration_request_origin_t::manual_request);
            CARROT_TEST_REQUIRE(texture_status->last_requested_action == assets::asset_runtime_refresh_action_t::reload_on_next_use);

            const auto sprite_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::sprite,
                                                                                  assets::make_asset_id("sprite.vraden")) };
            CARROT_TEST_REQUIRE(sprite_status.has_value());
            CARROT_TEST_REQUIRE(sprite_status->last_result == assets::asset_iteration_result_t::success);
            CARROT_TEST_REQUIRE(sprite_status->last_load_origin == assets::asset_load_origin_t::regenerated_from_source);
            CARROT_TEST_REQUIRE(sprite_status->last_refresh_request_origin ==
                                assets::asset_iteration_request_origin_t::manual_request);

            const auto audio_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::audio,
                                                                                 assets::make_asset_id("music.victory")) };
            CARROT_TEST_REQUIRE(audio_status.has_value());
            CARROT_TEST_REQUIRE(audio_status->last_result == assets::asset_iteration_result_t::success);
            CARROT_TEST_REQUIRE(audio_status->last_load_origin == assets::asset_load_origin_t::streamed_direct);
            CARROT_TEST_REQUIRE(audio_status->loaded_in_runtime_cache);
            CARROT_TEST_REQUIRE(audio_status->last_refresh_request_origin ==
                                assets::asset_iteration_request_origin_t::manual_request);
            CARROT_TEST_REQUIRE(audio_status->last_requested_action == assets::asset_runtime_refresh_action_t::reload_on_next_use);
        }

        void test_runtime_refresh_action_maps_reload_policy_to_runtime_action()
        {
            CARROT_TEST_REQUIRE(assets::recommended_runtime_refresh_action(
                                    assets::asset_reload_policy_t::reloadable_live,
                                    false) == assets::asset_runtime_refresh_action_t::reload_now);
            CARROT_TEST_REQUIRE(assets::recommended_runtime_refresh_action(
                                    assets::asset_reload_policy_t::reloadable_on_next_use,
                                    false) == assets::asset_runtime_refresh_action_t::reload_on_next_use);
            CARROT_TEST_REQUIRE(assets::recommended_runtime_refresh_action(
                                    assets::asset_reload_policy_t::manual_refresh_only,
                                    false) == assets::asset_runtime_refresh_action_t::manual_refresh);
            CARROT_TEST_REQUIRE(assets::recommended_runtime_refresh_action(
                                    assets::asset_reload_policy_t::restart_or_scene_rebuild_required,
                                    true) == assets::asset_runtime_refresh_action_t::rebuild_current_scene);
            CARROT_TEST_REQUIRE(assets::recommended_runtime_refresh_action(
                                    assets::asset_reload_policy_t::restart_or_scene_rebuild_required,
                                    false) == assets::asset_runtime_refresh_action_t::restart_runtime);
        }

        void test_runtime_refresh_action_uses_status_context_to_refine_recommendation()
        {
            assets::asset_iteration_status_t live_leaf_status;
            live_leaf_status.reload_policy = assets::asset_reload_policy_t::reloadable_live;
            live_leaf_status.dependency_shape = assets::asset_dependency_shape_t::leaf_runtime_data;
            live_leaf_status.loaded_in_runtime_cache = true;
            CARROT_TEST_REQUIRE(assets::recommended_runtime_refresh_action(
                                    live_leaf_status,
                                    false) == assets::asset_runtime_refresh_action_t::reload_now);

            live_leaf_status.loaded_in_runtime_cache = false;
            CARROT_TEST_REQUIRE(assets::recommended_runtime_refresh_action(
                                    live_leaf_status,
                                    false) == assets::asset_runtime_refresh_action_t::reload_on_next_use);

            assets::asset_iteration_status_t manual_streamed_status;
            manual_streamed_status.reload_policy = assets::asset_reload_policy_t::manual_refresh_only;
            manual_streamed_status.dependency_shape = assets::asset_dependency_shape_t::leaf_runtime_data;
            manual_streamed_status.loaded_in_runtime_cache = true;
            CARROT_TEST_REQUIRE(assets::recommended_runtime_refresh_action(
                                    manual_streamed_status,
                                    false) == assets::asset_runtime_refresh_action_t::manual_refresh);

            manual_streamed_status.loaded_in_runtime_cache = false;
            CARROT_TEST_REQUIRE(assets::recommended_runtime_refresh_action(
                                    manual_streamed_status,
                                    false) == assets::asset_runtime_refresh_action_t::reload_on_next_use);

            assets::asset_iteration_status_t structural_status;
            structural_status.reload_policy = assets::asset_reload_policy_t::restart_or_scene_rebuild_required;
            structural_status.dependency_shape = assets::asset_dependency_shape_t::scene_or_world_structure;
            CARROT_TEST_REQUIRE(assets::recommended_runtime_refresh_action(
                                    structural_status,
                                    true) == assets::asset_runtime_refresh_action_t::rebuild_current_scene);
            CARROT_TEST_REQUIRE(assets::recommended_runtime_refresh_action(
                                    structural_status,
                                    false) == assets::asset_runtime_refresh_action_t::restart_runtime);

            assets::asset_iteration_status_t presentation_status;
            presentation_status.reload_policy = assets::asset_reload_policy_t::restart_or_scene_rebuild_required;
            presentation_status.dependency_shape = assets::asset_dependency_shape_t::layout_or_presentation_contract;
            CARROT_TEST_REQUIRE(assets::recommended_runtime_refresh_action(
                                    presentation_status,
                                    true) == assets::asset_runtime_refresh_action_t::rebuild_current_scene);
            CARROT_TEST_REQUIRE(assets::describe_runtime_refresh_action_reason(
                                    presentation_status,
                                    true).find("presentation contracts") != std::string_view::npos);
        }

        void test_iteration_diagnostics_describe_watch_changes_and_attempts()
        {
            CARROT_TEST_REQUIRE(assets::detect_watch_change(true, false) ==
                                assets::asset_iteration_watch_change_t::source_timestamp_changed);
            CARROT_TEST_REQUIRE(assets::detect_watch_change(false, true) ==
                                assets::asset_iteration_watch_change_t::manifest_timestamp_changed);
            CARROT_TEST_REQUIRE(assets::detect_watch_change(true, true) ==
                                assets::asset_iteration_watch_change_t::source_and_manifest_timestamps_changed);

            assets::asset_iteration_status_t status;
            CARROT_TEST_REQUIRE(assets::describe_watch_change(status.last_watch_change).find("No watched source") !=
                                std::string_view::npos);
            CARROT_TEST_REQUIRE(assets::describe_last_attempt_summary(status).find("No runtime load") !=
                                std::string_view::npos);

            status.has_last_attempt = true;
            status.last_result = assets::asset_iteration_result_t::success;
            status.last_load_origin = assets::asset_load_origin_t::cooked_cache;
            CARROT_TEST_REQUIRE(assets::describe_last_attempt_summary(status).find("cooked cache") !=
                                std::string_view::npos);

            status.last_result = assets::asset_iteration_result_t::failed;
            CARROT_TEST_REQUIRE(assets::describe_last_attempt_summary(status).find("failed") !=
                                std::string_view::npos);

            CARROT_TEST_REQUIRE(assets::describe_invalidation_reason(
                                    assets::imported_artifact_issue_t::source_changed).find("source content changed") !=
                                std::string_view::npos);
        }

        void test_runtime_iteration_poll_reloads_live_texture_when_manifest_changes()
        {
            reset_auto_reload_root();

            const std::filesystem::path source_texture{ engine_assets_root() / "images" / "carrot_engine_logo_512.png" };
            const std::filesystem::path copied_texture{ auto_reload_root() / "game" / "textures" / "auto_reload_texture.png" };
            CARROT_TEST_REQUIRE(std::filesystem::copy_file(source_texture,
                                                           copied_texture,
                                                           std::filesystem::copy_options::overwrite_existing));

            const std::filesystem::path manifest_path{ auto_reload_root() / "game" / "textures" / "auto_reload.texture.json" };
            {
                std::ofstream out{ manifest_path };
                CARROT_TEST_REQUIRE(out.is_open());
                out << "{\n"
                       "  \"id\": \"texture.auto_reload\",\n"
                       "  \"source\": \"game://textures/auto_reload_texture.png\",\n"
                       "  \"srgb\": true\n"
                       "}\n";
            }

            io::virtual_file_system_t vfs;
            mount_auto_reload_roots(vfs);
            auto rhi{ make_null_rhi() };
            CARROT_TEST_REQUIRE(rhi != nullptr);
            assets::asset_manager_t asset_manager{ vfs, *rhi };

            auto doc{ parse_json("game://textures/auto_reload.texture.json", vfs) };
            CARROT_TEST_REQUIRE(assets::texture_asset_manifest_importer_t::import(doc,
                                                                                  asset_manager.textures().registry(),
                                                                                  vfs,
                                                                                  "game://textures/auto_reload.texture.json"));

            CARROT_TEST_REQUIRE(asset_manager.textures().get("texture.auto_reload") != nullptr);
            asset_manager.poll_runtime_iteration_changes();

            const auto before{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::texture,
                                                                           assets::make_asset_id("texture.auto_reload")) };
            CARROT_TEST_REQUIRE(before.has_value());
            CARROT_TEST_REQUIRE(before->has_last_attempt);

            const auto previous_attempt_time{ before->last_attempt_at };
            const auto current_write_time{ std::filesystem::last_write_time(manifest_path) };
            std::filesystem::last_write_time(manifest_path, current_write_time + std::chrono::seconds(2));

            asset_manager.poll_runtime_iteration_changes();

            const auto after{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::texture,
                                                                          assets::make_asset_id("texture.auto_reload")) };
            CARROT_TEST_REQUIRE(after.has_value());
            CARROT_TEST_REQUIRE(after->has_last_attempt);
            CARROT_TEST_REQUIRE(after->last_attempt_at > previous_attempt_time);
            CARROT_TEST_REQUIRE(after->last_result == assets::asset_iteration_result_t::success);
            CARROT_TEST_REQUIRE(after->loaded_in_runtime_cache);
            CARROT_TEST_REQUIRE(after->last_watch_change == assets::asset_iteration_watch_change_t::manifest_timestamp_changed);
            CARROT_TEST_REQUIRE(after->last_refresh_request_origin == assets::asset_iteration_request_origin_t::automatic_watch_poll);
            CARROT_TEST_REQUIRE(after->last_requested_action == assets::asset_runtime_refresh_action_t::reload_now);

            std::filesystem::remove_all(auto_reload_root());
        }

        void test_runtime_iteration_poll_reloads_live_texture_when_source_changes()
        {
            reset_auto_reload_root();

            const std::filesystem::path source_texture{ engine_assets_root() / "images" / "carrot_engine_logo_512.png" };
            const std::filesystem::path copied_texture{ auto_reload_root() / "game" / "textures" / "auto_reload_texture.png" };
            CARROT_TEST_REQUIRE(std::filesystem::copy_file(source_texture,
                                                           copied_texture,
                                                           std::filesystem::copy_options::overwrite_existing));

            const std::filesystem::path manifest_path{ auto_reload_root() / "game" / "textures" / "auto_reload.texture.json" };
            {
                std::ofstream out{ manifest_path };
                CARROT_TEST_REQUIRE(out.is_open());
                out << "{\n"
                       "  \"id\": \"texture.auto_reload\",\n"
                       "  \"source\": \"game://textures/auto_reload_texture.png\",\n"
                       "  \"srgb\": true\n"
                       "}\n";
            }

            io::virtual_file_system_t vfs;
            mount_auto_reload_roots(vfs);
            auto rhi{ make_null_rhi() };
            CARROT_TEST_REQUIRE(rhi != nullptr);
            assets::asset_manager_t asset_manager{ vfs, *rhi };

            auto doc{ parse_json("game://textures/auto_reload.texture.json", vfs) };
            CARROT_TEST_REQUIRE(assets::texture_asset_manifest_importer_t::import(doc,
                                                                                  asset_manager.textures().registry(),
                                                                                  vfs,
                                                                                  "game://textures/auto_reload.texture.json"));

            CARROT_TEST_REQUIRE(asset_manager.textures().get("texture.auto_reload") != nullptr);
            asset_manager.poll_runtime_iteration_changes();

            const auto before{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::texture,
                                                                           assets::make_asset_id("texture.auto_reload")) };
            CARROT_TEST_REQUIRE(before.has_value());
            CARROT_TEST_REQUIRE(before->has_last_attempt);

            const auto previous_attempt_time{ before->last_attempt_at };
            const auto current_write_time{ std::filesystem::last_write_time(copied_texture) };
            std::filesystem::last_write_time(copied_texture, current_write_time + std::chrono::seconds(2));

            asset_manager.poll_runtime_iteration_changes();

            const auto after{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::texture,
                                                                          assets::make_asset_id("texture.auto_reload")) };
            CARROT_TEST_REQUIRE(after.has_value());
            CARROT_TEST_REQUIRE(after->has_last_attempt);
            CARROT_TEST_REQUIRE(after->last_attempt_at > previous_attempt_time);
            CARROT_TEST_REQUIRE(after->last_result == assets::asset_iteration_result_t::success);
            CARROT_TEST_REQUIRE(after->loaded_in_runtime_cache);
            CARROT_TEST_REQUIRE(after->last_watch_change == assets::asset_iteration_watch_change_t::source_timestamp_changed);
            CARROT_TEST_REQUIRE(after->last_refresh_request_origin == assets::asset_iteration_request_origin_t::automatic_watch_poll);
            CARROT_TEST_REQUIRE(after->last_requested_action == assets::asset_runtime_refresh_action_t::reload_now);
            CARROT_TEST_REQUIRE(after->last_load_origin == assets::asset_load_origin_t::cooked_cache);

            std::filesystem::remove_all(auto_reload_root());
        }
    }

    void register_asset_iteration_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("AssetIteration/statuses expose registered assets",
                           test_runtime_iteration_statuses_expose_registered_assets);
        tests.emplace_back("AssetIteration/manual reload records last attempt details",
                           test_manual_reload_records_last_attempt_details);
        tests.emplace_back("AssetIteration/runtime refresh action maps reload policy to runtime action",
                           test_runtime_refresh_action_maps_reload_policy_to_runtime_action);
        tests.emplace_back("AssetIteration/runtime refresh action uses status context to refine recommendation",
                           test_runtime_refresh_action_uses_status_context_to_refine_recommendation);
        tests.emplace_back("AssetIteration/diagnostics describe watch changes and attempts",
                           test_iteration_diagnostics_describe_watch_changes_and_attempts);
        tests.emplace_back("AssetIteration/poll reloads live texture when manifest changes",
                           test_runtime_iteration_poll_reloads_live_texture_when_manifest_changes);
        tests.emplace_back("AssetIteration/poll reloads live texture when source changes",
                           test_runtime_iteration_poll_reloads_live_texture_when_source_changes);
    }
} // namespace carrot::tests
