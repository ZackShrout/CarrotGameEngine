//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "Assets/AssetDiscovery.h"
#include "Assets/AssetID.h"
#include "Assets/Font/CookedFont.h"
#include "Assets/Font/FontAssetLoader.h"
#include "Assets/Font/FontAssetManifestImporter.h"
#include "Assets/Font/FontAssetRegistry.h"
#include "Assets/Font/TextLayout.h"
#include "Assets/AssetManager.h"
#include "IO/VirtualFileSystem.h"
#include "RHI/Buffer.h"
#include "RHI/CommandQueue.h"
#include "RHI/RHI.h"
#include "RHI/Sampler.h"
#include "RHI/Texture.h"
#include "Utils/File/FileUtils.h"
#include "Utils/JSON/Public/JsonDocument.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string_view>
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

        [[nodiscard]] std::filesystem::path temp_save_root()
        {
            return std::filesystem::temp_directory_path() / "carrot_font_asset_save_root";
        }

        void mount_engine_assets(io::virtual_file_system_t& vfs)
        {
            vfs.mount("engine", engine_assets_root(), true);
        }

        void mount_engine_assets_with_save(io::virtual_file_system_t& vfs)
        {
            mount_engine_assets(vfs);
            vfs.mount("save", temp_save_root(), false);
        }

        [[nodiscard]] utils::json::json_document_t parse_json(const io::virtual_file_system_t& vfs, std::string_view manifest_uri)
        {
            const auto native_path{ vfs.resolve_native_path(manifest_uri) };
            CARROT_TEST_REQUIRE(native_path.has_value());

            utils::json::json_document_t doc;
            CARROT_TEST_REQUIRE(doc.parse_from_file(native_path->string().c_str()));
            return doc;
        }

        [[nodiscard]] assets::cooked_font_data_t make_test_cooked_font();

        void test_asset_discovery_finds_engine_font_manifests()
        {
            io::virtual_file_system_t vfs;
            mount_engine_assets(vfs);

            const auto manifests{ assets::asset_discovery_t::discover_supported_manifests(vfs) };
            CARROT_TEST_REQUIRE(manifests.fonts.size() >= 2u);
            CARROT_TEST_REQUIRE(std::find(manifests.fonts.begin(),
                                          manifests.fonts.end(),
                                          "engine://fonts/pixelnauts.font.json") != manifests.fonts.end());
            CARROT_TEST_REQUIRE(std::find(manifests.fonts.begin(),
                                          manifests.fonts.end(),
                                          "engine://fonts/roboto_regular.font.json") != manifests.fonts.end());
        }

        void test_font_manifest_importer_registers_engine_font_records()
        {
            io::virtual_file_system_t vfs;
            mount_engine_assets(vfs);

            assets::font_asset_registry_t registry;

            for (const std::string_view manifest_uri : {
                     std::string_view{ "engine://fonts/pixelnauts.font.json" },
                     std::string_view{ "engine://fonts/roboto_regular.font.json" } })
            {
                utils::json::json_document_t doc{ parse_json(vfs, manifest_uri) };
                CARROT_TEST_REQUIRE(assets::font_asset_manifest_importer_t::import(doc, registry, vfs, manifest_uri));
            }

            const assets::font_asset_record_t* pixelnauts{
                registry.find("font.engine.pixelnauts")
            };
            CARROT_TEST_REQUIRE(pixelnauts != nullptr);
            CARROT_TEST_REQUIRE(pixelnauts->source_uri == "engine://fonts/Pixelnauts.ttf");
            CARROT_TEST_REQUIRE(pixelnauts->manifest_uri == "engine://fonts/pixelnauts.font.json");
            CARROT_TEST_REQUIRE(pixelnauts->charset_preset == assets::font_charset_preset_t::basic_latin);
            CARROT_TEST_REQUIRE(pixelnauts->msdf.atlas_width == 1024u);
            CARROT_TEST_REQUIRE(pixelnauts->msdf.atlas_height == 1024u);
            CARROT_TEST_REQUIRE(pixelnauts->msdf.include_kerning);
            CARROT_TEST_REQUIRE(pixelnauts->defaults.line_height_scale == 1.0f);
            CARROT_TEST_REQUIRE(pixelnauts->id == assets::make_asset_id("font.engine.pixelnauts"));

            const assets::font_asset_record_t* roboto{
                registry.find("font.engine.roboto_regular")
            };
            CARROT_TEST_REQUIRE(roboto != nullptr);
            CARROT_TEST_REQUIRE(roboto->source_uri == "engine://fonts/Roboto-Regular.ttf");
            CARROT_TEST_REQUIRE(roboto->manifest_uri == "engine://fonts/roboto_regular.font.json");
        }

        void test_font_manifest_importer_rejects_unsupported_charset_preset()
        {
            io::virtual_file_system_t vfs;
            mount_engine_assets(vfs);

            assets::font_asset_registry_t registry;

            utils::json::json_document_t doc;
            constexpr std::string_view json{ R"({
                "version": 1,
                "id": "font.engine.bad_charset",
                "source": "engine://fonts/Roboto-Regular.ttf",
                "charset": {
                    "preset": "LatinExtended"
                },
                "msdf": {
                    "atlas_width": 1024,
                    "atlas_height": 1024,
                    "pixel_range": 8
                }
            })" };
            CARROT_TEST_REQUIRE(doc.parse_from_memory(json.data(), json.size()));

            CARROT_TEST_REQUIRE(
                !assets::font_asset_manifest_importer_t::import(doc,
                                                                registry,
                                                                vfs,
                                                                "engine://fonts/bad_charset.font.json"));
        }

        void test_font_asset_loader_generates_and_loads_cooked_font()
        {
            io::virtual_file_system_t vfs;
            mount_engine_assets_with_save(vfs);

            fake_context_t rhi;
            assets::asset_manager_t asset_manager{ vfs, rhi };

            {
                utils::json::json_document_t doc{ parse_json(vfs, "engine://fonts/roboto_regular.font.json") };
                CARROT_TEST_REQUIRE(assets::font_asset_manifest_importer_t::import(
                    doc,
                    asset_manager.fonts().registry(),
                    vfs,
                    "engine://fonts/roboto_regular.font.json"));
            }

            const assets::font_asset_record_t* record{
                asset_manager.fonts().registry().find("font.engine.roboto_regular")
            };
            CARROT_TEST_REQUIRE(record != nullptr);

            const assets::font_asset_load_result_t direct_load{
                assets::load_font_asset(*record, vfs, rhi)
            };
            CARROT_TEST_REQUIRE_MSG(direct_load.success(),
                                    std::string{ assets::to_string(direct_load.error) });

            const assets::loaded_font_asset_t* loaded{ asset_manager.fonts().get("font.engine.roboto_regular") };
            CARROT_TEST_REQUIRE(loaded != nullptr);
            CARROT_TEST_REQUIRE(loaded->valid());
            CARROT_TEST_REQUIRE(loaded->cooked.glyphs.size() == 95u);
            CARROT_TEST_REQUIRE(loaded->cooked.metrics.em_size > 0.0f);
            CARROT_TEST_REQUIRE(loaded->cooked.metrics.line_height > 0.0f);
            CARROT_TEST_REQUIRE(loaded->find_glyph(static_cast<std::uint32_t>('A')) != nullptr);
            CARROT_TEST_REQUIRE(loaded->find_glyph(static_cast<std::uint32_t>('A'))->advance > 0.0f);
            CARROT_TEST_REQUIRE(loaded->atlas_texture->width() == 1024u);
            CARROT_TEST_REQUIRE(loaded->atlas_texture->height() == 1024u);
            CARROT_TEST_REQUIRE(loaded->cooked.importer_version == 17u);
            CARROT_TEST_REQUIRE(loaded->cooked.atlas_payload.size() == (1024u * 1024u * 4u));
            CARROT_TEST_REQUIRE(std::any_of(loaded->cooked.atlas_payload.begin(),
                                            loaded->cooked.atlas_payload.end(),
                                            [](const std::uint8_t value) noexcept
                                            {
                                                return value != 0u;
                                            }));
            CARROT_TEST_REQUIRE(loaded->find_glyph(static_cast<std::uint32_t>('A'))->plane_left < 0.0f);
            CARROT_TEST_REQUIRE(std::any_of(loaded->cooked.atlas_payload.begin(),
                                            loaded->cooked.atlas_payload.end() - 3,
                                            [](const std::uint8_t value) noexcept
                                            {
                                                return value != 0u;
                                            }));
            CARROT_TEST_REQUIRE(loaded->find_glyph(static_cast<std::uint32_t>('A'))->uv_right >
                                loaded->find_glyph(static_cast<std::uint32_t>('A'))->uv_left);
            CARROT_TEST_REQUIRE(loaded->find_glyph(static_cast<std::uint32_t>('A'))->uv_bottom >
                                loaded->find_glyph(static_cast<std::uint32_t>('A'))->uv_top);

            const std::filesystem::path cooked_path{
                assets::cooked_font_cache_path("font.engine.roboto_regular", vfs)
            };
            CARROT_TEST_REQUIRE(!cooked_path.empty());
            CARROT_TEST_REQUIRE(std::filesystem::exists(cooked_path));

            std::filesystem::remove_all(temp_save_root());
        }

        void test_font_asset_loader_reuses_current_cooked_font()
        {
            io::virtual_file_system_t vfs;
            mount_engine_assets_with_save(vfs);

            fake_context_t rhi;
            assets::asset_manager_t asset_manager{ vfs, rhi };

            {
                utils::json::json_document_t doc{ parse_json(vfs, "engine://fonts/pixelnauts.font.json") };
                CARROT_TEST_REQUIRE(assets::font_asset_manifest_importer_t::import(
                    doc,
                    asset_manager.fonts().registry(),
                    vfs,
                    "engine://fonts/pixelnauts.font.json"));
            }

            const assets::loaded_font_asset_t* first{ asset_manager.fonts().get("font.engine.pixelnauts") };
            CARROT_TEST_REQUIRE(first != nullptr);

            const std::filesystem::path cooked_path{
                assets::cooked_font_cache_path("font.engine.pixelnauts", vfs)
            };
            CARROT_TEST_REQUIRE(std::filesystem::exists(cooked_path));
            const auto first_write_time{ std::filesystem::last_write_time(cooked_path) };

            asset_manager.fonts().clear_runtime_cache();
            const assets::loaded_font_asset_t* second{ asset_manager.fonts().get("font.engine.pixelnauts") };
            CARROT_TEST_REQUIRE(second != nullptr);
            const auto second_write_time{ std::filesystem::last_write_time(cooked_path) };
            CARROT_TEST_REQUIRE(second_write_time == first_write_time);
            CARROT_TEST_REQUIRE(second->cooked.metrics.em_size > 0.0f);
            CARROT_TEST_REQUIRE(second->find_glyph(static_cast<std::uint32_t>('A')) != nullptr);

            std::filesystem::remove_all(temp_save_root());
        }

        void test_loaded_font_asset_supports_glyph_lookup_and_kerning()
        {
            const assets::cooked_font_data_t source{ make_test_cooked_font() };
            assets::loaded_font_asset_t loaded{};
            loaded.record = reinterpret_cast<const assets::font_asset_record_t*>(0x1);
            loaded.cooked = source;
            loaded.atlas_texture = std::make_unique<fake_texture_t>(rhi::texture_create_info_t{
                .width = 2u,
                .height = 2u,
                .format = rhi::texture_format_t::rgba8_unorm,
            });

            const assets::cfont_glyph_record_t* glyph_a{ loaded.find_glyph(65u) };
            CARROT_TEST_REQUIRE(glyph_a != nullptr);
            CARROT_TEST_REQUIRE(glyph_a->glyph_index == 1u);
            CARROT_TEST_REQUIRE(loaded.find_glyph(999u) == nullptr);
            CARROT_TEST_REQUIRE(loaded.kerning_adjustment(65u, 86u) == -1.25f);
            CARROT_TEST_REQUIRE(loaded.kerning_adjustment(65u, 65u) == 0.0f);
        }

        void test_text_layout_measures_and_positions_glyphs()
        {
            const assets::cooked_font_data_t source{ make_test_cooked_font() };
            assets::loaded_font_asset_t loaded{};
            loaded.record = reinterpret_cast<const assets::font_asset_record_t*>(0x1);
            loaded.cooked = source;
            loaded.atlas_texture = std::make_unique<fake_texture_t>(rhi::texture_create_info_t{
                .width = 2u,
                .height = 2u,
                .format = rhi::texture_format_t::rgba8_unorm,
            });

            const assets::text_layout_result_t layout{
                assets::layout_text(loaded,
                                    "AB",
                                    assets::text_layout_settings_t{
                                        .font_size = 32.0f,
                                        .wrap_width = 0.0f,
                                        .letter_spacing = 0.0f,
                                        .line_spacing = 0.0f,
                                    })
            };

            CARROT_TEST_REQUIRE(layout.line_count == 1u);
            CARROT_TEST_REQUIRE(layout.glyphs.size() == 2u);
            CARROT_TEST_REQUIRE(layout.glyphs[0].codepoint == 65u);
            CARROT_TEST_REQUIRE(layout.glyphs[1].x > layout.glyphs[0].x);
            CARROT_TEST_REQUIRE(layout.bounds.width > 0.0f);
            CARROT_TEST_REQUIRE(layout.bounds.height > 0.0f);
        }

        void test_text_layout_wraps_and_counts_lines()
        {
            const assets::cooked_font_data_t source{ make_test_cooked_font() };
            assets::loaded_font_asset_t loaded{};
            loaded.record = reinterpret_cast<const assets::font_asset_record_t*>(0x1);
            loaded.cooked = source;
            loaded.atlas_texture = std::make_unique<fake_texture_t>(rhi::texture_create_info_t{
                .width = 2u,
                .height = 2u,
                .format = rhi::texture_format_t::rgba8_unorm,
            });

            const assets::text_layout_result_t layout{
                assets::layout_text(loaded,
                                    "ABA",
                                    assets::text_layout_settings_t{
                                        .font_size = 32.0f,
                                        .wrap_width = 20.0f,
                                        .letter_spacing = 0.0f,
                                        .line_spacing = 4.0f,
                                    })
            };

            CARROT_TEST_REQUIRE(layout.line_count >= 2u);
            CARROT_TEST_REQUIRE(layout.bounds.height > 32.0f);
        }

        void test_binary_file_utils_write_and_read_round_trip()
        {
            utils::file::binary_blob_writer_t writer;
            const size_t offset_u8{ writer.write_u8(0xABu) };
            const size_t offset_u32{ writer.write_u32(0x12345678u) };
            const size_t offset_f32{ writer.write_f32(3.5f) };
            const size_t aligned_size{ writer.align(16u) };
            CARROT_TEST_REQUIRE(offset_u8 == 0u);
            CARROT_TEST_REQUIRE(offset_u32 == 1u);
            CARROT_TEST_REQUIRE(offset_f32 == 5u);
            CARROT_TEST_REQUIRE(aligned_size == 16u);

            const std::filesystem::path temp_root{
                std::filesystem::temp_directory_path() / "carrot_font_tests_binary_writer"
            };
            const std::filesystem::path output_path{ temp_root / "blob.bin" };

            CARROT_TEST_REQUIRE(utils::file::write_binary_file(output_path, writer.data()));

            const auto loaded{ utils::file::load_binary_file(output_path) };
            CARROT_TEST_REQUIRE(loaded.has_value());
            CARROT_TEST_REQUIRE(loaded->size() == writer.size());
            CARROT_TEST_REQUIRE((*loaded)[0] == 0xABu);
            CARROT_TEST_REQUIRE((*loaded)[1] == 0x78u);
            CARROT_TEST_REQUIRE((*loaded)[2] == 0x56u);
            CARROT_TEST_REQUIRE((*loaded)[3] == 0x34u);
            CARROT_TEST_REQUIRE((*loaded)[4] == 0x12u);

            std::filesystem::remove_all(temp_root);
        }

        [[nodiscard]] assets::cooked_font_data_t make_test_cooked_font()
        {
            assets::cooked_font_data_t font;
            font.importer_version = 7u;
            font.invalidation.source_font_content_hash = 11u;
            font.invalidation.asset_definition_content_hash = 22u;
            font.invalidation.import_settings_hash = 33u;

            font.metrics.em_size = 32.0f;
            font.metrics.line_height = 40.0f;
            font.metrics.ascent = 28.0f;
            font.metrics.descent = -8.0f;
            font.metrics.underline_position = -3.0f;
            font.metrics.underline_thickness = 1.5f;
            font.metrics.atlas_width = 2u;
            font.metrics.atlas_height = 2u;
            font.metrics.atlas_format = rhi::texture_format_t::rgba8_unorm;
            font.metrics.atlas_channel_layout = 1u;
            font.metrics.msdf_pixel_range = 8.0f;
            font.metrics.distance_normalization = 1.0f;

            font.glyphs = {
                {
                    .codepoint = 65u,
                    .glyph_index = 1u,
                    .advance = 14.0f,
                    .plane_left = 0.0f,
                    .plane_top = 12.0f,
                    .plane_right = 10.0f,
                    .plane_bottom = -2.0f,
                    .uv_left = 0.0f,
                    .uv_top = 0.0f,
                    .uv_right = 0.5f,
                    .uv_bottom = 0.5f,
                },
                {
                    .codepoint = 66u,
                    .glyph_index = 2u,
                    .advance = 13.0f,
                    .plane_left = 0.0f,
                    .plane_top = 12.0f,
                    .plane_right = 9.0f,
                    .plane_bottom = -2.0f,
                    .uv_left = 0.5f,
                    .uv_top = 0.0f,
                    .uv_right = 1.0f,
                    .uv_bottom = 0.5f,
                }
            };

            font.kerning_pairs = {
                {
                    .left_codepoint = 65u,
                    .right_codepoint = 86u,
                    .adjustment = -1.25f,
                }
            };

            font.atlas_payload = {
                255u, 0u,   0u,   255u,
                0u,   255u, 0u,   255u,
                0u,   0u,   255u, 255u,
                255u, 255u, 255u, 255u,
            };

            return font;
        }

        void test_cooked_font_serialization_round_trips()
        {
            const assets::cooked_font_data_t source{ make_test_cooked_font() };
            const auto serialized{ assets::serialize_cooked_font(source) };
            CARROT_TEST_REQUIRE(serialized.has_value());

            const auto loaded{ assets::deserialize_cooked_font(*serialized) };
            CARROT_TEST_REQUIRE(loaded.has_value());
            CARROT_TEST_REQUIRE(loaded->importer_version == 7u);
            CARROT_TEST_REQUIRE(loaded->glyphs.size() == 2u);
            CARROT_TEST_REQUIRE(loaded->kerning_pairs.size() == 1u);
            CARROT_TEST_REQUIRE(loaded->glyphs[0].codepoint == 65u);
            CARROT_TEST_REQUIRE(loaded->glyphs[1].glyph_index == 2u);
            CARROT_TEST_REQUIRE(loaded->kerning_pairs[0].right_codepoint == 86u);
            CARROT_TEST_REQUIRE(loaded->metrics.atlas_width == 2u);
            CARROT_TEST_REQUIRE(loaded->atlas_payload == source.atlas_payload);
        }

        void test_cooked_font_file_round_trip()
        {
            const assets::cooked_font_data_t source{ make_test_cooked_font() };

            const std::filesystem::path temp_root{
                std::filesystem::temp_directory_path() / "carrot_font_tests_cfont"
            };
            const std::filesystem::path output_path{ temp_root / "test.cfont" };

            CARROT_TEST_REQUIRE(assets::write_cooked_font_file(output_path, source));

            const auto loaded{ assets::load_cooked_font_file(output_path) };
            CARROT_TEST_REQUIRE(loaded.has_value());
            CARROT_TEST_REQUIRE(loaded->metrics.line_height == 40.0f);
            CARROT_TEST_REQUIRE(loaded->glyphs.size() == source.glyphs.size());
            CARROT_TEST_REQUIRE(loaded->atlas_payload.size() == source.atlas_payload.size());

            std::filesystem::remove_all(temp_root);
        }

        void test_cooked_font_rejects_bad_magic()
        {
            const assets::cooked_font_data_t source{ make_test_cooked_font() };
            auto serialized{ assets::serialize_cooked_font(source) };
            CARROT_TEST_REQUIRE(serialized.has_value());
            (*serialized)[0] = 'X';

            CARROT_TEST_REQUIRE(!assets::deserialize_cooked_font(*serialized).has_value());
        }
    } // namespace

    void register_font_asset_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("binary file utils write and read round trip",
                           &test_binary_file_utils_write_and_read_round_trip);
        tests.emplace_back("asset discovery finds engine font manifests", &test_asset_discovery_finds_engine_font_manifests);
        tests.emplace_back("font manifest importer registers engine font records",
                           &test_font_manifest_importer_registers_engine_font_records);
        tests.emplace_back("font manifest importer rejects unsupported charset preset",
                           &test_font_manifest_importer_rejects_unsupported_charset_preset);
        tests.emplace_back("font asset loader generates and loads cooked font",
                           &test_font_asset_loader_generates_and_loads_cooked_font);
        tests.emplace_back("font asset loader reuses current cooked font",
                           &test_font_asset_loader_reuses_current_cooked_font);
        tests.emplace_back("loaded font asset supports glyph lookup and kerning",
                           &test_loaded_font_asset_supports_glyph_lookup_and_kerning);
        tests.emplace_back("text layout measures and positions glyphs",
                           &test_text_layout_measures_and_positions_glyphs);
        tests.emplace_back("text layout wraps and counts lines",
                           &test_text_layout_wraps_and_counts_lines);
        tests.emplace_back("cooked font serialization round trips",
                           &test_cooked_font_serialization_round_trips);
        tests.emplace_back("cooked font file round trip",
                           &test_cooked_font_file_round_trip);
        tests.emplace_back("cooked font rejects bad magic",
                           &test_cooked_font_rejects_bad_magic);
    }
} // namespace carrot::tests
