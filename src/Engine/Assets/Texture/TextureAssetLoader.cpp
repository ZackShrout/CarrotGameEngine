//
// Created by Zack Shrout on 3/21/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TextureAssetLoader.h"

#include "Assets/Image/ImageAssetImporter.h"
#include "Assets/ImportedAssetCache.h"
#include "CookedTexture.h"
#include "IO/VirtualFileSystem.h"
#include "RHI/RHI.h"
#include "Utils/File/FileUtils.h"

namespace carrot::assets {
    namespace {
        constexpr std::uint32_t texture_importer_version{ 1u };

        template<typename T>
        void hash_append_value(std::uint64_t& hash, const T& value) noexcept
        {
            const auto bytes{
                std::as_bytes(std::span<const T, 1>{ &value, 1 })
            };
            hash_append_bytes(hash,
                              std::span<const std::uint8_t>{
                                  reinterpret_cast<const std::uint8_t*>(bytes.data()),
                                  bytes.size()
                              });
        }

        void hash_append_string(std::uint64_t& hash, const std::string_view value) noexcept
        {
            hash_append_bytes(hash,
                              std::span<const std::uint8_t>{
                                  reinterpret_cast<const std::uint8_t*>(value.data()),
                                  value.size()
                              });
        }

        [[nodiscard]] std::uint64_t compute_import_settings_hash(const texture_asset_record_t& record) noexcept
        {
            std::uint64_t hash{ 14695981039346656037ull };
            hash_append_value(hash, record.schema_version);
            hash_append_value(hash, static_cast<std::uint8_t>(record.srgb ? 1u : 0u));
            hash_append_string(hash, record.logical_id);
            hash_append_string(hash, record.source_uri);
            return hash;
        }

        [[nodiscard]] imported_asset_invalidation_t build_expected_invalidation(const texture_asset_record_t& record,
                                                                                const io::virtual_file_system_t& vfs)
        {
            imported_asset_invalidation_t invalidation;
            invalidation.import_settings_hash = compute_import_settings_hash(record);

            if (const auto source_hash{ hash_vfs_file_contents(vfs, record.source_uri) })
                invalidation.source_content_hash = *source_hash;

            if (!record.manifest_uri.empty())
            {
                if (const auto manifest_hash{ hash_vfs_file_contents(vfs, record.manifest_uri) })
                    invalidation.asset_definition_content_hash = *manifest_hash;
            }

            return invalidation;
        }

        [[nodiscard]] texture_asset_load_result_t create_runtime_texture(const texture_asset_record_t& record,
                                                                         const cooked_texture_data_t& cooked,
                                                                         rhi::rhi_context_t& rhi) noexcept
        {
            rhi::texture_create_info_t texture_info{ };
            texture_info.width = cooked.width;
            texture_info.height = cooked.height;
            texture_info.format = cooked.format;
            texture_info.initial_data = cooked.pixel_payload.data();
            texture_info.initial_data_size = cooked.pixel_payload.size();
            texture_info.initial_data_stride_bytes = cooked.stride_bytes;

            std::unique_ptr<rhi::rhi_texture_t> texture{ rhi.create_texture_2d(texture_info) };
            if (!texture)
                return { { }, texture_asset_load_error::texture_create_failed };

            loaded_texture_asset_t loaded{ };
            loaded.record = &record;
            loaded.texture = std::move(texture);
            return { std::move(loaded), texture_asset_load_error::ok };
        }
    } // namespace

    std::filesystem::path cooked_texture_cache_path(const std::string_view logical_id,
                                                    const io::virtual_file_system_t& vfs) noexcept
    {
        return imported_asset_cache_path(logical_id, "textures", ".ctex", vfs);
    }

    texture_asset_load_result_t load_texture_asset(const texture_asset_record_t& record,
                                                   const io::virtual_file_system_t& vfs,
                                                   rhi::rhi_context_t& rhi) noexcept
    {
        if (record.id == 0 || record.logical_id.empty() || record.source_uri.empty())
            return { { }, texture_asset_load_error::invalid_record };

        const auto native_path{ vfs.resolve_native_path(record.source_uri) };
        if (!native_path)
            return { { }, texture_asset_load_error::resolve_failed };

        if (!std::filesystem::exists(*native_path))
            return { { }, texture_asset_load_error::source_not_found };

        if (!record.manifest_uri.empty())
        {
            const auto manifest_path{ vfs.resolve_native_path(record.manifest_uri) };
            if (!manifest_path || !std::filesystem::exists(*manifest_path))
                return { { }, texture_asset_load_error::manifest_not_found };
        }

        const imported_asset_invalidation_t expected_invalidation{
            build_expected_invalidation(record, vfs)
        };

        imported_artifact_issue_t invalidation_reason{ imported_artifact_issue_t::missing_artifact };
        imported_artifact_state_t cooked_artifact_state{ imported_artifact_state_t::missing };
        const std::filesystem::path cooked_path{ cooked_texture_cache_path(record.logical_id, vfs) };
        if (!cooked_path.empty() && std::filesystem::exists(cooked_path))
        {
            const auto cooked{ load_cooked_texture_file(cooked_path) };
            if (!cooked)
            {
                LOG_ASSET_WARN("Texture asset '{}' has unreadable cooked artifact '{}'; regenerating.",
                               record.logical_id,
                               cooked_path.string());
                invalidation_reason = imported_artifact_issue_t::unreadable_artifact;
            }
            else
            {
                cooked_artifact_state = inspect_imported_artifact_state(cooked->invalidation,
                                                                       expected_invalidation,
                                                                       cooked->importer_version,
                                                                       texture_importer_version,
                                                                       invalidation_reason);
                if (cooked_artifact_state == imported_artifact_state_t::valid)
                {
                    LOG_ASSET_INFO("Loaded texture asset '{}' from cooked cache", record.logical_id);
                    auto result{ create_runtime_texture(record, *cooked, rhi) };
                    result.load_origin = asset_load_origin_t::cooked_cache;
                    result.cooked_artifact_state = imported_artifact_state_t::valid;
                    result.invalidation_reason = imported_artifact_issue_t::none;
                    return result;
                }

                LOG_ASSET_INFO("Cooked texture '{}' is stale; regenerating '{}'.",
                               cooked_path.string(),
                               record.logical_id);
            }
        }

        image_load_result_t image_result{ load_image_rgba8(*native_path) };
        if (!image_result.success())
            return { { }, texture_asset_load_error::decode_failed, asset_load_origin_t::never_loaded, cooked_artifact_state, invalidation_reason };

        cooked_texture_data_t cooked{ };
        cooked.importer_version = texture_importer_version;
        cooked.invalidation = expected_invalidation;
        cooked.width = image_result.image.width;
        cooked.height = image_result.image.height;
        cooked.stride_bytes = image_result.image.stride_bytes;
        cooked.format = record.srgb ? rhi::texture_format_t::rgba8_srgb : rhi::texture_format_t::rgba8_unorm;
        cooked.pixel_payload = image_result.image.pixels;

        auto runtime_texture_result{ create_runtime_texture(record, cooked, rhi) };
        if (!runtime_texture_result.success())
        {
            runtime_texture_result.cooked_artifact_state = cooked_artifact_state;
            runtime_texture_result.invalidation_reason = invalidation_reason;
            return runtime_texture_result;
        }

        runtime_texture_result.invalidation_reason = invalidation_reason;
        if (!cooked_path.empty())
        {
            const auto serialized{ serialize_cooked_texture(cooked) };
            if (!serialized)
                return { { }, texture_asset_load_error::cooked_serialize_failed, asset_load_origin_t::never_loaded, cooked_artifact_state, invalidation_reason };

            if (!utils::file::write_binary_file(cooked_path, *serialized))
                return { { }, texture_asset_load_error::cooked_write_failed, asset_load_origin_t::never_loaded, cooked_artifact_state, invalidation_reason };

            LOG_ASSET_INFO("Regenerated cooked texture '{}' for asset '{}'.",
                           cooked_path.string(),
                           record.logical_id);
            runtime_texture_result.load_origin = asset_load_origin_t::regenerated_from_source;
            runtime_texture_result.cooked_artifact_state = cooked_artifact_state;
        }
        else
        {
            LOG_ASSET_INFO("Texture asset '{}' loaded from source without cooked cache (no save mount).",
                           record.logical_id);
            runtime_texture_result.load_origin = asset_load_origin_t::source_without_cooked_cache;
            runtime_texture_result.cooked_artifact_state = imported_artifact_state_t::missing;
        }

        return runtime_texture_result;
    }
} // namespace carrot::assets
