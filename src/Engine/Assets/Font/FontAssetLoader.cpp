//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "FontAssetLoader.h"

#include "Assets/ImportedAssetCache.h"
#include "IO/VirtualFileSystem.h"
#include "MSDFAtlasWrapper.h"
#include "RHI/RHI.h"
#include "Utils/File/FileUtils.h"

#include <bit>

namespace carrot::assets {
    namespace {
        constexpr std::uint32_t font_importer_version{ 17u };
        constexpr std::uint32_t basic_latin_first{ 32u };
        constexpr std::uint32_t basic_latin_last{ 126u };

        [[nodiscard]] bool validate_sorted_glyphs(const std::vector<cfont_glyph_record_t>& glyphs) noexcept
        {
            for (size_t i{ 1u }; i < glyphs.size(); ++i)
            {
                if (glyphs[i - 1u].codepoint >= glyphs[i].codepoint)
                    return false;
            }

            return true;
        }

        [[nodiscard]] bool validate_sorted_kerning(const std::vector<cfont_kerning_pair_t>& kerning_pairs) noexcept
        {
            for (size_t i{ 1u }; i < kerning_pairs.size(); ++i)
            {
                const auto& prev{ kerning_pairs[i - 1u] };
                const auto& curr{ kerning_pairs[i] };
                if (prev.left_codepoint > curr.left_codepoint)
                    return false;
                if (prev.left_codepoint == curr.left_codepoint && prev.right_codepoint >= curr.right_codepoint)
                    return false;
            }

            return true;
        }

        [[nodiscard]] font_asset_load_error_t validate_cooked_font_for_loading(const cooked_font_data_t& font) noexcept
        {
            if (font.cooked_format_version != 1u)
                return font_asset_load_error_t::cooked_invalid_format;

            if (font.metrics.atlas_width == 0u || font.metrics.atlas_height == 0u)
                return font_asset_load_error_t::cooked_invalid_atlas_dimensions;

            if (font.metrics.msdf_pixel_range <= 0.0f || font.metrics.distance_normalization <= 0.0f)
                return font_asset_load_error_t::cooked_invalid_distance_settings;

            if (!validate_sorted_glyphs(font.glyphs) || !validate_sorted_kerning(font.kerning_pairs))
                return font_asset_load_error_t::cooked_invalid_tables;

            if (font.metrics.atlas_format != rhi::texture_format_t::rgba8_unorm &&
                font.metrics.atlas_format != rhi::texture_format_t::rgba8_srgb)
            {
                return font_asset_load_error_t::cooked_invalid_atlas_format;
            }

            const size_t expected_payload_size{
                static_cast<size_t>(font.metrics.atlas_width) *
                static_cast<size_t>(font.metrics.atlas_height) * 4u
            };
            if (font.atlas_payload.size() != expected_payload_size)
                return font_asset_load_error_t::cooked_invalid_atlas_payload;

            return font_asset_load_error_t::ok;
        }

        [[nodiscard]] std::uint64_t fnv1a_append(std::uint64_t hash, std::span<const std::uint8_t> bytes) noexcept
        {
            constexpr std::uint64_t fnv_prime{ 1099511628211ull };
            for (const std::uint8_t byte : bytes)
            {
                hash ^= static_cast<std::uint64_t>(byte);
                hash *= fnv_prime;
            }

            return hash;
        }

        template<typename T>
        void hash_value(std::uint64_t& hash, const T& value) noexcept
        {
            const auto bytes{
                std::as_bytes(std::span<const T, 1>{ &value, 1 })
            };
            hash = fnv1a_append(hash, std::span<const std::uint8_t>{
                reinterpret_cast<const std::uint8_t*>(bytes.data()),
                bytes.size()
            });
        }

        void hash_string(std::uint64_t& hash, const std::string_view value) noexcept
        {
            hash = fnv1a_append(hash, std::span<const std::uint8_t>{
                reinterpret_cast<const std::uint8_t*>(value.data()),
                value.size()
            });
        }

        [[nodiscard]] std::uint64_t hash_bytes(const std::vector<std::uint8_t>& bytes) noexcept
        {
            constexpr std::uint64_t fnv_offset{ 14695981039346656037ull };
            return fnv1a_append(fnv_offset, bytes);
        }

        [[nodiscard]] std::uint64_t compute_import_settings_hash(const font_asset_record_t& record) noexcept
        {
            std::uint64_t hash{ 14695981039346656037ull };
            hash_value(hash, record.schema_version);
            hash_value(hash, static_cast<std::uint8_t>(record.charset_preset));
            hash_value(hash, record.msdf.atlas_width);
            hash_value(hash, record.msdf.atlas_height);
            hash_value(hash, std::bit_cast<std::uint32_t>(record.msdf.pixel_range));
            hash_value(hash, static_cast<std::uint8_t>(record.msdf.include_kerning ? 1u : 0u));
            hash_value(hash, std::bit_cast<std::uint32_t>(record.defaults.line_height_scale));
            hash_string(hash, record.logical_id);

            for (const std::uint32_t codepoint : record.include_codepoints)
                hash_value(hash, codepoint);

            for (const std::uint32_t codepoint : record.exclude_codepoints)
                hash_value(hash, codepoint);

            return hash;
        }

        [[nodiscard]] bool contains_codepoint(const std::vector<std::uint32_t>& codepoints,
                                              const std::uint32_t codepoint) noexcept
        {
            return std::binary_search(codepoints.begin(), codepoints.end(), codepoint);
        }

        [[nodiscard]] std::vector<std::uint32_t> build_codepoint_set(const font_asset_record_t& record)
        {
            std::vector<std::uint32_t> codepoints;

            switch (record.charset_preset)
            {
                case font_charset_preset_t::basic_latin:
                    for (std::uint32_t cp{ basic_latin_first }; cp <= basic_latin_last; ++cp)
                        codepoints.push_back(cp);
                    break;
            }

            for (const std::uint32_t include_cp : record.include_codepoints)
            {
                if (!contains_codepoint(codepoints, include_cp))
                    codepoints.push_back(include_cp);
            }

            std::sort(codepoints.begin(), codepoints.end());
            codepoints.erase(std::remove_if(codepoints.begin(),
                                            codepoints.end(),
                                            [&record](const std::uint32_t cp) noexcept
                                            {
                                                return contains_codepoint(record.exclude_codepoints, cp);
                                            }),
                             codepoints.end());
            return codepoints;
        }

    } // namespace

    std::filesystem::path cooked_font_cache_path(const std::string_view logical_id,
                                                 const io::virtual_file_system_t& vfs) noexcept
    {
        const auto save_mount{ vfs.get_mount("save") };
        if (!save_mount)
            return {};

        return save_mount->root / "cache" / "fonts" / (std::string{ logical_id } + ".cfont");
    }

    font_asset_load_result_t load_font_asset(const font_asset_record_t& record,
                                             const io::virtual_file_system_t& vfs,
                                             rhi::rhi_context_t& rhi) noexcept
    {
        if (record.id == 0 || record.logical_id.empty() || record.source_uri.empty() || record.manifest_uri.empty())
            return { {}, font_asset_load_error_t::invalid_record };

        const auto source_path{ vfs.resolve_native_path(record.source_uri) };
        if (!source_path)
            return { {}, font_asset_load_error_t::source_not_found };

        const auto manifest_path{ vfs.resolve_native_path(record.manifest_uri) };
        if (!manifest_path)
            return { {}, font_asset_load_error_t::manifest_not_found };

        const auto save_mount{ vfs.get_mount("save") };
        if (!save_mount)
            return { {}, font_asset_load_error_t::save_mount_unavailable };

        const auto source_bytes{ utils::file::load_binary_file(*source_path) };
        const auto manifest_bytes{ utils::file::load_binary_file(*manifest_path) };
        if (!source_bytes)
            return { {}, font_asset_load_error_t::source_not_found };
        if (!manifest_bytes)
            return { {}, font_asset_load_error_t::manifest_not_found };

        const std::uint64_t source_hash{ hash_bytes(*source_bytes) };
        const std::uint64_t manifest_hash{ hash_bytes(*manifest_bytes) };
        const std::filesystem::path cooked_path{ cooked_font_cache_path(record.logical_id, vfs) };
        asset_load_origin_t load_origin{ asset_load_origin_t::never_loaded };
        imported_artifact_state_t cooked_artifact_state{ imported_artifact_state_t::missing };
        imported_artifact_issue_t invalidation_reason{ imported_artifact_issue_t::none };

        std::optional<cooked_font_data_t> cooked;
        if (!cooked_path.empty() && std::filesystem::exists(cooked_path))
        {
            cooked = load_cooked_font_file(cooked_path);
            if (cooked)
            {
                const std::uint64_t expected_settings_hash{ compute_import_settings_hash(record) };
                if (cooked->importer_version != font_importer_version)
                {
                    cooked_artifact_state = imported_artifact_state_t::stale;
                    invalidation_reason = imported_artifact_issue_t::importer_version_changed;
                }
                else if (cooked->invalidation.source_font_content_hash != source_hash)
                {
                    cooked_artifact_state = imported_artifact_state_t::stale;
                    invalidation_reason = imported_artifact_issue_t::source_changed;
                }
                else if (cooked->invalidation.asset_definition_content_hash != manifest_hash)
                {
                    cooked_artifact_state = imported_artifact_state_t::stale;
                    invalidation_reason = imported_artifact_issue_t::asset_definition_changed;
                }
                else if (cooked->invalidation.import_settings_hash != expected_settings_hash)
                {
                    cooked_artifact_state = imported_artifact_state_t::stale;
                    invalidation_reason = imported_artifact_issue_t::import_settings_changed;
                }
                else if (cooked->invalidation.reserved_hash != 0u)
                {
                    cooked_artifact_state = imported_artifact_state_t::stale;
                    invalidation_reason = imported_artifact_issue_t::reserved_changed;
                }
                else
                {
                    cooked_artifact_state = imported_artifact_state_t::valid;
                    invalidation_reason = imported_artifact_issue_t::none;
                }

                if (cooked_artifact_state == imported_artifact_state_t::valid)
                {
                    load_origin = asset_load_origin_t::cooked_cache;
                }
                else
                {
                    cooked.reset();
                }
            }
            else
            {
                cooked_artifact_state = imported_artifact_state_t::stale;
                invalidation_reason = imported_artifact_issue_t::unreadable_artifact;
            }
        }

        if (!cooked)
        {
            const auto codepoints{ build_codepoint_set(record) };
            auto generated{ generate_cooked_font_with_msdf_atlas(record,
                                                                 *source_bytes,
                                                                 codepoints,
                                                                 source_hash,
                                                                 manifest_hash,
                                                                 font_importer_version) };
            if (!generated)
            {
                LOG_ASSET_ERROR("Font asset '{}' failed to generate cooked atlas", record.logical_id);
                return { {}, font_asset_load_error_t::generator_failed };
            }

            cooked_font_data_t rebuilt{ std::move(*generated) };
            rebuilt.invalidation.import_settings_hash = compute_import_settings_hash(record);

            const font_asset_load_error_t validation_result{
                validate_cooked_font_for_loading(rebuilt)
            };
            if (validation_result != font_asset_load_error_t::ok)
                return { {}, validation_result };

            const auto serialized{ serialize_cooked_font(rebuilt) };
            if (!serialized)
                return { {}, font_asset_load_error_t::cooked_serialize_failed };

            if (!utils::file::write_binary_file(cooked_path, *serialized))
                return { {}, font_asset_load_error_t::cooked_write_failed };

            cooked = load_cooked_font_file(cooked_path);
            if (!cooked)
                return { {}, font_asset_load_error_t::cooked_read_failed };

            load_origin = asset_load_origin_t::regenerated_from_source;
            cooked_artifact_state = imported_artifact_state_t::valid;
            invalidation_reason = imported_artifact_issue_t::none;
        }

        rhi::texture_create_info_t texture_info{};
        texture_info.width = cooked->metrics.atlas_width;
        texture_info.height = cooked->metrics.atlas_height;
        texture_info.format = cooked->metrics.atlas_format;
        texture_info.initial_data = cooked->atlas_payload.data();
        texture_info.initial_data_size = cooked->atlas_payload.size();
        texture_info.initial_data_stride_bytes = cooked->metrics.atlas_width * 4u;

        std::unique_ptr<rhi::rhi_texture_t> atlas_texture{ rhi.create_texture_2d(texture_info) };
        if (!atlas_texture)
            return { {}, font_asset_load_error_t::texture_create_failed };

        loaded_font_asset_t loaded{};
        loaded.record = &record;
        loaded.cooked = std::move(*cooked);
        loaded.atlas_texture = std::move(atlas_texture);

        LOG_ASSET_INFO("Loaded font asset '{}' from {}",
                       record.logical_id,
                       load_origin == asset_load_origin_t::cooked_cache ? "cooked cache" : "generated data");
        return {
            std::move(loaded),
            font_asset_load_error_t::ok,
            load_origin,
            cooked_artifact_state,
            invalidation_reason
        };
    }
} // namespace carrot::assets
