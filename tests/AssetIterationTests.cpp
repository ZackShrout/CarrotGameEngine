//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "Assets/AssetManager.h"
#include "Assets/Audio/AudioAssetManifestImporter.h"
#include "Assets/Sprite/SpriteAssetManifestImporter.h"
#include "Assets/Texture/TextureAssetManifestImporter.h"
#include "IO/VirtualFileSystem.h"
#include "RHI/Buffer.h"
#include "RHI/CommandQueue.h"
#include "RHI/RHI.h"
#include "RHI/Sampler.h"
#include "RHI/Texture.h"
#include "Utils/JSON/Public/JsonDocument.h"

#include <filesystem>
#include <functional>
#include <string_view>
#include <unordered_map>
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
            uint32_t _width{ 0u };
            uint32_t _height{ 0u };
            rhi::texture_format_t _format{ rhi::texture_format_t::rgba8_unorm };
        };

        class fake_buffer_t final : public rhi::rhi_buffer_t
        {
        public:
            explicit fake_buffer_t(const rhi::buffer_create_info_t& info) noexcept
                : rhi::rhi_buffer_t{ info.size_bytes, info.usage } {}

            [[nodiscard]] bool write([[maybe_unused]] const void* data,
                                     [[maybe_unused]] const size_t size_bytes,
                                     [[maybe_unused]] const size_t offset_bytes = 0) override
            {
                return true;
            }
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
            void record_text_quad_stage([[maybe_unused]] const rhi::textured_quad_stage_record_t& stage) override {}
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
                (void)inserted;
                return it->second.get();
            }

            void bind_textured_quad_resources([[maybe_unused]] const rhi::rhi_texture_t& texture,
                                              [[maybe_unused]] const rhi::rhi_sampler_t& sampler) override {}

            void wait_idle() override {}

        private:
            fake_command_queue_t _queue;
            std::unordered_map<rhi::sampler_desc_t, std::unique_ptr<fake_sampler_t>, rhi::sampler_desc_hash_t> _samplers;
        };

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

        void reset_temp_root()
        {
            std::filesystem::remove_all(temp_root());
            std::filesystem::create_directories(temp_root() / "save");
        }

        void mount_all(io::virtual_file_system_t& vfs)
        {
            vfs.mount("engine", engine_assets_root(), true);
            vfs.mount("game", game_assets_root(), true);
            vfs.mount("save", temp_root() / "save", false);
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
        }

        void test_runtime_iteration_statuses_expose_registered_assets()
        {
            reset_temp_root();

            io::virtual_file_system_t vfs;
            mount_all(vfs);
            fake_context_t rhi;
            assets::asset_manager_t asset_manager{ vfs, rhi };
            register_runtime_iteration_assets(asset_manager, vfs);

            const auto statuses{ asset_manager.collect_runtime_iteration_statuses() };
            CARROT_TEST_REQUIRE(statuses.size() == 4u);

            const auto texture_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::texture,
                                                                                   assets::make_asset_id("engine.carrot_engine_logo_512")) };
            CARROT_TEST_REQUIRE(texture_status.has_value());
            CARROT_TEST_REQUIRE(texture_status->reload_policy == assets::asset_reload_policy_t::reloadable_live);
            CARROT_TEST_REQUIRE(!texture_status->has_last_attempt);

            const auto audio_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::audio,
                                                                                 assets::make_asset_id("music.victory")) };
            CARROT_TEST_REQUIRE(audio_status.has_value());
            CARROT_TEST_REQUIRE(audio_status->reload_policy == assets::asset_reload_policy_t::manual_refresh_only);
        }

        void test_manual_reload_records_last_attempt_details()
        {
            reset_temp_root();

            io::virtual_file_system_t vfs;
            mount_all(vfs);
            fake_context_t rhi;
            assets::asset_manager_t asset_manager{ vfs, rhi };
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

            const auto sprite_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::sprite,
                                                                                  assets::make_asset_id("sprite.vraden")) };
            CARROT_TEST_REQUIRE(sprite_status.has_value());
            CARROT_TEST_REQUIRE(sprite_status->last_result == assets::asset_iteration_result_t::success);
            CARROT_TEST_REQUIRE(sprite_status->last_load_origin == assets::asset_load_origin_t::regenerated_from_source);

            const auto audio_status{ asset_manager.find_runtime_iteration_status(assets::asset_kind_t::audio,
                                                                                 assets::make_asset_id("music.victory")) };
            CARROT_TEST_REQUIRE(audio_status.has_value());
            CARROT_TEST_REQUIRE(audio_status->last_result == assets::asset_iteration_result_t::success);
            CARROT_TEST_REQUIRE(audio_status->last_load_origin == assets::asset_load_origin_t::streamed_direct);
            CARROT_TEST_REQUIRE(audio_status->loaded_in_runtime_cache);
        }
    }

    void register_asset_iteration_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("AssetIteration/statuses expose registered assets",
                           test_runtime_iteration_statuses_expose_registered_assets);
        tests.emplace_back("AssetIteration/manual reload records last attempt details",
                           test_manual_reload_records_last_attempt_details);
    }
} // namespace carrot::tests
