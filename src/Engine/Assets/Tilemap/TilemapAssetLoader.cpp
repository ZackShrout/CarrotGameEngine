//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TilemapAssetLoader.h"

#include "Assets/ImportedAssetCache.h"
#include "Assets/Image/ImageAssetImporter.h"
#include "CookedTilemap.h"
#include "IO/VirtualFileSystem.h"
#include "RHI/RHI.h"
#include "Utils/File/FileUtils.h"

namespace carrot::assets {
    namespace {
        constexpr std::uint32_t tilemap_importer_version{ 1u };

        [[nodiscard]] std::filesystem::path normalize_tileset_image_path(std::string_view raw_path)
        {
            std::string normalized{ raw_path };
            std::replace(normalized.begin(), normalized.end(), '\\', '/');
            return std::filesystem::path{ normalized };
        }

        template<typename T>
        void hash_append_value(std::uint64_t& hash, const T& value) noexcept
        {
            const auto bytes{ std::as_bytes(std::span<const T, 1>{ &value, 1 }) };
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

        [[nodiscard]] std::uint64_t compute_import_settings_hash(const tilemap_asset_record_t& record) noexcept
        {
            std::uint64_t hash{ 14695981039346656037ull };
            hash_append_value(hash, record.schema_version);
            hash_append_string(hash, record.logical_id);
            hash_append_string(hash, record.source_uri);
            return hash;
        }

        [[nodiscard]] imported_asset_invalidation_t build_expected_invalidation(const tilemap_asset_record_t& record,
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
    }

    std::filesystem::path cooked_tilemap_cache_path(const std::string_view logical_id,
                                                    const io::virtual_file_system_t& vfs) noexcept
    {
        return imported_asset_cache_path(logical_id, "tilemaps", ".cmap", vfs);
    }

    tilemap_asset_load_result_t load_tilemap_asset(const tilemap_asset_record_t& record)
    {
        if (record.logical_id.empty())
        {
            return {
                .asset = { },
                .error = tilemap_asset_load_error_t::invalid_record
            };
        }

        return {
            .asset = loaded_tilemap_asset_t{ record.tilemap, &record },
            .error = tilemap_asset_load_error_t::none
        };
    }

    tilemap_asset_load_result_t load_tilemap_asset(const tilemap_asset_record_t& record,
                                                   const io::virtual_file_system_t& vfs,
                                                   rhi::rhi_context_t& rhi) noexcept
    {
        if (!record.manifest_uri.empty())
        {
            const auto manifest_path{ vfs.resolve_native_path(record.manifest_uri) };
            if (!manifest_path || !std::filesystem::exists(*manifest_path))
            {
                return {
                    .asset = { },
                    .error = tilemap_asset_load_error_t::manifest_not_found
                };
            }
        }

        tilemap_asset_record_t working_record{ record };
        bool loaded_from_cooked{ false };
        const imported_asset_invalidation_t expected_invalidation{
            build_expected_invalidation(record, vfs)
        };
        const std::filesystem::path cooked_path{ cooked_tilemap_cache_path(record.logical_id, vfs) };
        if (!cooked_path.empty() && std::filesystem::exists(cooked_path))
        {
            const auto cooked{ load_cooked_tilemap_file(cooked_path) };
            if (cooked && is_imported_asset_current(cooked->invalidation,
                                                   expected_invalidation,
                                                   cooked->importer_version,
                                                   tilemap_importer_version))
            {
                working_record.tilemap = cooked->tilemap;
                loaded_from_cooked = true;
                LOG_ASSET_INFO("Loaded tilemap asset '{}' from cooked cache", record.logical_id);
            }
        }

        tilemap_asset_load_result_t result{ load_tilemap_asset(working_record) };
        if (!result.success())
            return result;

        const auto source_path{ vfs.resolve_native_path(working_record.source_uri) };
        if (!source_path)
        {
            result.error = tilemap_asset_load_error_t::resolve_failed;
            return result;
        }

        for (const tilemap_tileset_t& tileset : working_record.tilemap.tilesets())
        {
            if (tileset.image_source_uri.empty())
            {
                result.asset.add_tileset_texture(nullptr);
                continue;
            }

            std::filesystem::path image_path{ normalize_tileset_image_path(tileset.image_source_uri) };
            if (!image_path.is_absolute())
                image_path = source_path->parent_path() / image_path;

            image_path = image_path.lexically_normal();

            if (!std::filesystem::exists(image_path))
            {
                LOG_ASSET_ERROR("Tilemap tileset image not found: raw='{}', resolved='{}'",
                                tileset.image_source_uri, image_path.string());
                result.error = tilemap_asset_load_error_t::source_not_found;
                return result;
            }

            image_load_result_t image_result{ load_image_rgba8(image_path) };
            if (!image_result.success())
            {
                result.error = tilemap_asset_load_error_t::decode_failed;
                return result;
            }

            rhi::texture_create_info_t texture_info{ };
            texture_info.width = image_result.image.width;
            texture_info.height = image_result.image.height;
            texture_info.format = image_result.image.is_srgb ? rhi::texture_format_t::rgba8_srgb
                                                             : rhi::texture_format_t::rgba8_unorm;
            texture_info.initial_data = image_result.image.data();
            texture_info.initial_data_size = image_result.image.size_bytes();
            texture_info.initial_data_stride_bytes = image_result.image.stride_bytes;

            std::unique_ptr<rhi::rhi_texture_t> texture{ rhi.create_texture_2d(texture_info) };
            if (!texture)
            {
                result.error = tilemap_asset_load_error_t::texture_create_failed;
                return result;
            }

            result.asset.add_tileset_texture(std::move(texture));
        }

        if (!loaded_from_cooked && !cooked_path.empty())
        {
            cooked_tilemap_data_t cooked;
            cooked.importer_version = tilemap_importer_version;
            cooked.invalidation = expected_invalidation;
            cooked.tilemap = working_record.tilemap;

            if (!write_cooked_tilemap_file(cooked_path, cooked))
            {
                result.error = tilemap_asset_load_error_t::cooked_write_failed;
                return result;
            }
        }

        return result;
    }
} // namespace carrot::assets
