//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "Assets/AssetManager.h"
#include "Assets/Audio/AudioAssetLoader.h"
#include "Assets/Audio/AudioAssetManifestImporter.h"
#include "Assets/Audio/CookedAudio.h"
#include "Assets/Sprite/CookedSprite.h"
#include "Assets/Sprite/SpriteAssetLoader.h"
#include "Assets/Sprite/SpriteAssetManifestImporter.h"
#include "Assets/Tilemap/CookedTilemap.h"
#include "Assets/Tilemap/TilemapAssetLoader.h"
#include "Assets/Tilemap/TilemapAssetManifestImporter.h"
#include "Assets/Texture/CookedTexture.h"
#include "Assets/Texture/TextureAssetLoader.h"
#include "Assets/Texture/TextureAssetManifestImporter.h"
#include "IO/VirtualFileSystem.h"
#include "RHI/RHI.h"
#include "Utils/JSON/Public/JsonDocument.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>
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

        [[nodiscard]] std::filesystem::path engine_assets_root()
        {
            return std::filesystem::path{ CARROT_SOURCE_ROOT } / "assets";
        }

        [[nodiscard]] std::filesystem::path temp_root()
        {
            return std::filesystem::temp_directory_path() / "carrot_cooked_asset_tests";
        }

        [[nodiscard]] std::filesystem::path temp_game_root()
        {
            return temp_root() / "game";
        }

        [[nodiscard]] std::filesystem::path temp_save_root()
        {
            return temp_root() / "save";
        }

        void mount_all(io::virtual_file_system_t& vfs)
        {
            vfs.mount("engine", engine_assets_root(), true);
            vfs.mount("game", temp_game_root(), false);
            vfs.mount("save", temp_save_root(), false);
        }

        void cleanup_temp_roots()
        {
            std::filesystem::remove_all(temp_root());
        }

        void write_text_file(const std::filesystem::path& path, const std::string_view content)
        {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream stream{ path, std::ios::binary | std::ios::trunc };
            CARROT_TEST_REQUIRE(stream.is_open());
            stream.write(content.data(), static_cast<std::streamsize>(content.size()));
            stream.close();
            CARROT_TEST_REQUIRE(stream.good());
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

        [[nodiscard]] assets::cooked_texture_data_t make_test_cooked_texture()
        {
            assets::cooked_texture_data_t texture;
            texture.importer_version = 7u;
            texture.invalidation.source_content_hash = 11u;
            texture.invalidation.asset_definition_content_hash = 22u;
            texture.invalidation.import_settings_hash = 33u;
            texture.width = 2u;
            texture.height = 2u;
            texture.stride_bytes = 8u;
            texture.format = rhi::texture_format_t::rgba8_srgb;
            texture.pixel_payload = {
                255u, 0u,   0u,   255u,
                0u,   255u, 0u,   255u,
                0u,   0u,   255u, 255u,
                255u, 255u, 255u, 255u,
            };
            return texture;
        }

        [[nodiscard]] assets::cooked_audio_data_t make_test_cooked_audio()
        {
            assets::cooked_audio_data_t audio;
            audio.importer_version = 9u;
            audio.invalidation.source_content_hash = 101u;
            audio.invalidation.asset_definition_content_hash = 202u;
            audio.invalidation.import_settings_hash = 303u;
            audio.sample_rate = 48000u;
            audio.channels = 2u;
            audio.frame_count = 3u;
            audio.pcm_payload = { 0.1f, -0.1f, 0.25f, -0.25f, 0.5f, -0.5f };
            return audio;
        }

        [[nodiscard]] assets::cooked_sprite_data_t make_test_cooked_sprite()
        {
            assets::cooked_sprite_data_t cooked;
            cooked.importer_version = 5u;
            cooked.invalidation.source_content_hash = 1u;
            cooked.invalidation.asset_definition_content_hash = 2u;
            cooked.invalidation.import_settings_hash = 3u;
            cooked.sprite.set_texture_id("engine.carrot_engine_logo_512");
            cooked.sprite.set_default_pivot({ 0.5f, 0.75f });
            cooked.sprite.set_pixels_per_unit(16.0f);
            cooked.sprite.add_frame({
                .name = "idle",
                .pixel_rect = { .position = { 0u, 0u }, .size = { 16u, 16u } },
                .pivot = { 0.5f, 0.75f }
            });
            assets::sprite_animation_t animation;
            animation.name = "idle_anim";
            animation.loop = true;
            animation.frames.push_back({ .frame_index = 0u, .duration_seconds = 0.1f });
            cooked.sprite.add_animation(std::move(animation));
            CARROT_TEST_REQUIRE(cooked.sprite.build_lookup_tables());
            return cooked;
        }

        [[nodiscard]] assets::cooked_tilemap_data_t make_test_cooked_tilemap()
        {
            assets::cooked_tilemap_data_t cooked;
            cooked.importer_version = 6u;
            cooked.invalidation.source_content_hash = 10u;
            cooked.invalidation.asset_definition_content_hash = 20u;
            cooked.invalidation.import_settings_hash = 30u;
            cooked.tilemap.set_orientation(assets::tilemap_orientation_t::orthogonal);
            cooked.tilemap.set_size(2u, 1u);
            cooked.tilemap.set_tile_size(16u, 16u);
            cooked.tilemap.set_source_format("carrot.ctilemap.json");
            cooked.tilemap.add_property({ .name = "theme", .value = std::string{ "test" } });

            assets::tilemap_layer_t layer;
            layer.kind = assets::tilemap_layer_kind_t::tile;
            layer.name = "base";
            layer.width = 2u;
            layer.height = 1u;
            layer.gids = { 1u, 2u };
            cooked.tilemap.add_layer(std::move(layer));
            return cooked;
        }

        void test_cooked_texture_round_trip()
        {
            const assets::cooked_texture_data_t source{ make_test_cooked_texture() };
            const auto serialized{ assets::serialize_cooked_texture(source) };
            CARROT_TEST_REQUIRE(serialized.has_value());

            const auto loaded{ assets::deserialize_cooked_texture(*serialized) };
            CARROT_TEST_REQUIRE(loaded.has_value());
            CARROT_TEST_REQUIRE(loaded->importer_version == 7u);
            CARROT_TEST_REQUIRE(loaded->width == 2u);
            CARROT_TEST_REQUIRE(loaded->height == 2u);
            CARROT_TEST_REQUIRE(loaded->format == rhi::texture_format_t::rgba8_srgb);
            CARROT_TEST_REQUIRE(loaded->pixel_payload == source.pixel_payload);
        }

        void test_cooked_audio_round_trip()
        {
            const assets::cooked_audio_data_t source{ make_test_cooked_audio() };
            const auto serialized{ assets::serialize_cooked_audio(source) };
            CARROT_TEST_REQUIRE(serialized.has_value());

            const auto loaded{ assets::deserialize_cooked_audio(*serialized) };
            CARROT_TEST_REQUIRE(loaded.has_value());
            CARROT_TEST_REQUIRE(loaded->importer_version == 9u);
            CARROT_TEST_REQUIRE(loaded->sample_rate == 48000u);
            CARROT_TEST_REQUIRE(loaded->channels == 2u);
            CARROT_TEST_REQUIRE(loaded->frame_count == 3u);
            CARROT_TEST_REQUIRE(loaded->pcm_payload == source.pcm_payload);
        }

        void test_cooked_sprite_round_trip()
        {
            const assets::cooked_sprite_data_t source{ make_test_cooked_sprite() };
            const auto serialized{ assets::serialize_cooked_sprite(source) };
            CARROT_TEST_REQUIRE(serialized.has_value());

            const auto loaded{ assets::deserialize_cooked_sprite(*serialized) };
            CARROT_TEST_REQUIRE(loaded.has_value());
            CARROT_TEST_REQUIRE(loaded->importer_version == 5u);
            CARROT_TEST_REQUIRE(loaded->sprite.texture_id() == "engine.carrot_engine_logo_512");
            CARROT_TEST_REQUIRE(loaded->sprite.frames().size() == 1u);
            CARROT_TEST_REQUIRE(loaded->sprite.animations().size() == 1u);
            CARROT_TEST_REQUIRE(loaded->sprite.find_frame("idle") != nullptr);
        }

        void test_cooked_tilemap_round_trip()
        {
            const assets::cooked_tilemap_data_t source{ make_test_cooked_tilemap() };
            const auto serialized{ assets::serialize_cooked_tilemap(source) };
            CARROT_TEST_REQUIRE(serialized.has_value());

            const auto loaded{ assets::deserialize_cooked_tilemap(*serialized) };
            CARROT_TEST_REQUIRE(loaded.has_value());
            CARROT_TEST_REQUIRE(loaded->importer_version == 6u);
            CARROT_TEST_REQUIRE(loaded->tilemap.width() == 2u);
            CARROT_TEST_REQUIRE(loaded->tilemap.layers().size() == 1u);
            CARROT_TEST_REQUIRE(loaded->tilemap.layers()[0].gids.size() == 2u);
        }

        void test_texture_manifest_importer_records_manifest_uri()
        {
            cleanup_temp_roots();

            io::virtual_file_system_t vfs;
            mount_all(vfs);
            assets::texture_asset_registry_t registry;

            utils::json::json_document_t doc{ parse_json("engine://textures/carrot_engine_logo_512.texture.json", vfs) };
            CARROT_TEST_REQUIRE(assets::texture_asset_manifest_importer_t::import(
                doc,
                registry,
                vfs,
                "engine://textures/carrot_engine_logo_512.texture.json"));

            const auto* record{ registry.find("engine.carrot_engine_logo_512") };
            CARROT_TEST_REQUIRE(record != nullptr);
            CARROT_TEST_REQUIRE(record->manifest_uri == "engine://textures/carrot_engine_logo_512.texture.json");
            CARROT_TEST_REQUIRE(record->schema_version == 1u);

            cleanup_temp_roots();
        }

        void test_audio_manifest_importer_records_manifest_uri()
        {
            cleanup_temp_roots();

            io::virtual_file_system_t vfs;
            mount_all(vfs);
            assets::audio_asset_registry_t registry;

            utils::json::json_document_t doc{ parse_json("engine://audio/victory.audio.json", vfs) };
            CARROT_TEST_REQUIRE(assets::audio_asset_manifest_importer_t::import(
                doc,
                registry,
                vfs,
                "engine://audio/victory.audio.json"));

            const auto* record{ registry.find("music.victory") };
            CARROT_TEST_REQUIRE(record != nullptr);
            CARROT_TEST_REQUIRE(record->manifest_uri == "engine://audio/victory.audio.json");
            CARROT_TEST_REQUIRE(record->schema_version == 1u);

            cleanup_temp_roots();
        }

        void test_texture_asset_loader_reuses_and_invalidates_cooked_texture()
        {
            cleanup_temp_roots();

            constexpr std::string_view manifest_uri{ "game://textures/runtime_logo.texture.json" };
            const std::filesystem::path manifest_path{ temp_game_root() / "textures" / "runtime_logo.texture.json" };
            write_text_file(manifest_path,
                            R"({
                                "version": 1,
                                "id": "game.runtime_logo",
                                "source": "engine://images/carrot_engine_logo_512.png",
                                "srgb": true
                            })");

            io::virtual_file_system_t vfs;
            mount_all(vfs);

            fake_context_t rhi;
            assets::asset_manager_t asset_manager{ vfs, rhi };

            utils::json::json_document_t doc{ parse_json(manifest_uri, vfs) };
            CARROT_TEST_REQUIRE(assets::texture_asset_manifest_importer_t::import(
                doc,
                asset_manager.textures().registry(),
                vfs,
                manifest_uri));

            const assets::loaded_texture_asset_t* first{ asset_manager.textures().get("game.runtime_logo") };
            CARROT_TEST_REQUIRE(first != nullptr);
            CARROT_TEST_REQUIRE(first->valid());

            const std::filesystem::path cooked_path{
                assets::cooked_texture_cache_path("game.runtime_logo", vfs)
            };
            CARROT_TEST_REQUIRE(std::filesystem::exists(cooked_path));
            const auto first_write_time{ std::filesystem::last_write_time(cooked_path) };

            asset_manager.textures().clear_runtime_cache();
            const assets::loaded_texture_asset_t* second{ asset_manager.textures().get("game.runtime_logo") };
            CARROT_TEST_REQUIRE(second != nullptr);
            CARROT_TEST_REQUIRE(std::filesystem::last_write_time(cooked_path) == first_write_time);

            std::this_thread::sleep_for(std::chrono::milliseconds(1100));
            write_text_file(manifest_path,
                            R"({
                                "version": 1,
                                "id": "game.runtime_logo",
                                "source": "engine://images/carrot_engine_logo_512.png",
                                "srgb": false
                            })");

            asset_manager.textures().clear_runtime_cache();
            const assets::loaded_texture_asset_t* third{ asset_manager.textures().get("game.runtime_logo") };
            CARROT_TEST_REQUIRE(third != nullptr);
            CARROT_TEST_REQUIRE(std::filesystem::last_write_time(cooked_path) > first_write_time);

            cleanup_temp_roots();
        }

        void test_audio_asset_loader_generates_and_reuses_cooked_audio()
        {
            cleanup_temp_roots();

            constexpr std::string_view manifest_uri{ "game://audio/runtime_victory.audio.json" };
            const std::filesystem::path manifest_path{ temp_game_root() / "audio" / "runtime_victory.audio.json" };
            write_text_file(manifest_path,
                            R"({
                                "version": 1,
                                "id": "audio.runtime_victory",
                                "source": "engine://audio/Victory!.wav",
                                "streamed": false,
                                "looping": false,
                                "spatial": "none"
                            })");

            io::virtual_file_system_t vfs;
            mount_all(vfs);

            fake_context_t rhi;
            assets::asset_manager_t asset_manager{ vfs, rhi };

            utils::json::json_document_t doc{ parse_json(manifest_uri, vfs) };
            CARROT_TEST_REQUIRE(assets::audio_asset_manifest_importer_t::import(
                doc,
                asset_manager.audio().registry(),
                vfs,
                manifest_uri));

            const assets::loaded_audio_asset_t* first{ asset_manager.audio().get("audio.runtime_victory") };
            CARROT_TEST_REQUIRE(first != nullptr);
            CARROT_TEST_REQUIRE(first->valid());
            CARROT_TEST_REQUIRE(first->sample != nullptr);
            CARROT_TEST_REQUIRE(first->sample->frame_count > 0u);

            const std::filesystem::path cooked_path{
                assets::cooked_audio_cache_path("audio.runtime_victory", vfs)
            };
            CARROT_TEST_REQUIRE(std::filesystem::exists(cooked_path));
            const auto first_write_time{ std::filesystem::last_write_time(cooked_path) };

            asset_manager.audio().clear_runtime_cache();
            const assets::loaded_audio_asset_t* second{ asset_manager.audio().get("audio.runtime_victory") };
            CARROT_TEST_REQUIRE(second != nullptr);
            CARROT_TEST_REQUIRE(second->sample != nullptr);
            CARROT_TEST_REQUIRE(std::filesystem::last_write_time(cooked_path) == first_write_time);

            cleanup_temp_roots();
        }

        void test_sprite_asset_loader_generates_and_reuses_cooked_sprite()
        {
            cleanup_temp_roots();

            const std::filesystem::path sprite_source_path{ temp_game_root() / "sprites" / "runtime_sprite.csprite.json" };
            const std::filesystem::path sprite_manifest_path{ temp_game_root() / "sprites" / "runtime_sprite.sprite.json" };
            write_text_file(sprite_source_path,
                            R"({
                                "pixels_per_unit": 16,
                                "frames": {
                                    "idle": { "x": 0, "y": 0, "w": 16, "h": 16 }
                                },
                                "animations": {
                                    "idle_anim": {
                                        "loop": true,
                                        "frames": [
                                            { "frame": "idle", "duration": 0.1 }
                                        ]
                                    }
                                }
                            })");
            write_text_file(sprite_manifest_path,
                            R"({
                                "version": 1,
                                "id": "sprite.runtime.test",
                                "texture": "engine.carrot_engine_logo_512",
                                "source": "game://sprites/runtime_sprite.csprite.json"
                            })");

            io::virtual_file_system_t vfs;
            mount_all(vfs);

            fake_context_t rhi;
            assets::asset_manager_t asset_manager{ vfs, rhi };

            utils::json::json_document_t texture_doc{ parse_json("engine://textures/carrot_engine_logo_512.texture.json", vfs) };
            CARROT_TEST_REQUIRE(assets::texture_asset_manifest_importer_t::import(
                texture_doc,
                asset_manager.textures().registry(),
                vfs,
                "engine://textures/carrot_engine_logo_512.texture.json"));

            utils::json::json_document_t sprite_doc{ parse_json("game://sprites/runtime_sprite.sprite.json", vfs) };
            CARROT_TEST_REQUIRE(assets::sprite_asset_manifest_importer_t::import(
                sprite_doc,
                asset_manager.sprites().registry(),
                vfs,
                "game://sprites/runtime_sprite.sprite.json"));

            const assets::loaded_sprite_asset_t* first{ asset_manager.sprites().get("sprite.runtime.test") };
            CARROT_TEST_REQUIRE(first != nullptr);
            CARROT_TEST_REQUIRE(first->valid());
            CARROT_TEST_REQUIRE(first->find_animation("idle_anim") != nullptr);

            const std::filesystem::path cooked_path{
                assets::cooked_sprite_cache_path("sprite.runtime.test", vfs)
            };
            CARROT_TEST_REQUIRE(std::filesystem::exists(cooked_path));
            CARROT_TEST_REQUIRE(assets::load_cooked_sprite_file(cooked_path).has_value());

            asset_manager.sprites().clear_runtime_cache();
            const assets::loaded_sprite_asset_t* second{ asset_manager.sprites().get("sprite.runtime.test") };
            CARROT_TEST_REQUIRE(second != nullptr);
            CARROT_TEST_REQUIRE(second->find_frame("idle") != nullptr);

            cleanup_temp_roots();
        }

        void test_tilemap_asset_loader_generates_and_reuses_cooked_tilemap()
        {
            cleanup_temp_roots();

            const std::filesystem::path tilemap_source_path{ temp_game_root() / "maps" / "runtime_map.ctilemap.json" };
            const std::filesystem::path tilemap_manifest_path{ temp_game_root() / "maps" / "runtime_map.tilemap.json" };
            write_text_file(tilemap_source_path,
                            R"({
                                "width": 2,
                                "height": 1,
                                "tile_width": 16,
                                "tile_height": 16,
                                "layers": [
                                    {
                                        "kind": "tile",
                                        "name": "base",
                                        "width": 2,
                                        "height": 1,
                                        "gids": [1, 2]
                                    }
                                ]
                            })");
            write_text_file(tilemap_manifest_path,
                            R"({
                                "version": 1,
                                "id": "tilemap.runtime.test",
                                "source": "game://maps/runtime_map.ctilemap.json"
                            })");

            io::virtual_file_system_t vfs;
            mount_all(vfs);

            fake_context_t rhi;
            assets::asset_manager_t asset_manager{ vfs, rhi };

            utils::json::json_document_t tilemap_doc{ parse_json("game://maps/runtime_map.tilemap.json", vfs) };
            CARROT_TEST_REQUIRE(assets::tilemap_asset_manifest_importer_t::import(
                tilemap_doc,
                asset_manager.tilemaps().registry(),
                vfs,
                "game://maps/runtime_map.tilemap.json"));

            const assets::loaded_tilemap_asset_t* first{ asset_manager.tilemaps().get("tilemap.runtime.test") };
            CARROT_TEST_REQUIRE(first != nullptr);
            CARROT_TEST_REQUIRE(first->valid());
            CARROT_TEST_REQUIRE(first->tilemap().layers().size() == 1u);

            const std::filesystem::path cooked_path{
                assets::cooked_tilemap_cache_path("tilemap.runtime.test", vfs)
            };
            CARROT_TEST_REQUIRE(std::filesystem::exists(cooked_path));
            CARROT_TEST_REQUIRE(assets::load_cooked_tilemap_file(cooked_path).has_value());

            asset_manager.tilemaps().clear_runtime_cache();
            const assets::loaded_tilemap_asset_t* second{ asset_manager.tilemaps().get("tilemap.runtime.test") };
            CARROT_TEST_REQUIRE(second != nullptr);
            CARROT_TEST_REQUIRE(second->tilemap().layers().size() == 1u);

            cleanup_temp_roots();
        }
    } // namespace

    void register_asset_cooked_pipeline_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("cooked texture round trip", &test_cooked_texture_round_trip);
        tests.emplace_back("cooked audio round trip", &test_cooked_audio_round_trip);
        tests.emplace_back("cooked sprite round trip", &test_cooked_sprite_round_trip);
        tests.emplace_back("cooked tilemap round trip", &test_cooked_tilemap_round_trip);
        tests.emplace_back("texture manifest importer records manifest uri",
                           &test_texture_manifest_importer_records_manifest_uri);
        tests.emplace_back("audio manifest importer records manifest uri",
                           &test_audio_manifest_importer_records_manifest_uri);
        tests.emplace_back("texture asset loader reuses and invalidates cooked texture",
                           &test_texture_asset_loader_reuses_and_invalidates_cooked_texture);
        tests.emplace_back("audio asset loader generates and reuses cooked audio",
                           &test_audio_asset_loader_generates_and_reuses_cooked_audio);
        tests.emplace_back("sprite asset loader generates and reuses cooked sprite",
                           &test_sprite_asset_loader_generates_and_reuses_cooked_sprite);
        tests.emplace_back("tilemap asset loader generates and reuses cooked tilemap",
                           &test_tilemap_asset_loader_generates_and_reuses_cooked_tilemap);
    }
} // namespace carrot::tests
