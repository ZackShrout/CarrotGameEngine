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
        constexpr std::uint32_t tilemap_importer_version{ 4u };

        [[nodiscard]] std::filesystem::path normalize_tileset_image_path(std::string_view raw_path)
        {
            std::string normalized{ raw_path };
            std::replace(normalized.begin(), normalized.end(), '\\', '/');
            return std::filesystem::path{ normalized };
        }

        [[nodiscard]] std::optional<std::filesystem::path> resolve_tileset_base_path(
            const tilemap_asset_record_t& record,
            const tilemap_tileset_t& tileset,
            const io::virtual_file_system_t& vfs) noexcept
        {
            if (!tileset.source_uri.empty())
                return vfs.resolve_native_path(tileset.source_uri);

            return vfs.resolve_native_path(record.source_uri);
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
        const tilemap_asset_prepare_result_t prepared{ prepare_tilemap_asset(record, vfs) };
        if (!prepared.success())
            return { .asset = { }, .error = prepared.error };

        return realize_prepared_tilemap_asset(prepared_tilemap_asset_t{ prepared.asset }, rhi);
    }

    tilemap_asset_prepare_result_t prepare_tilemap_asset(const tilemap_asset_record_t& record,
                                                         const io::virtual_file_system_t& vfs) noexcept
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
            return { .asset = { }, .error = result.error };

        prepared_tilemap_asset_t prepared;
        prepared.tilemap = result.asset.tilemap();
        prepared.record = &record;
        prepared.expected_invalidation = expected_invalidation;
        prepared.cooked_path = cooked_path;
        prepared.write_cooked_artifact = !loaded_from_cooked && !cooked_path.empty();
        prepared.tileset_textures.reserve(working_record.tilemap.tilesets().size());

        for (const tilemap_tileset_t& tileset : working_record.tilemap.tilesets())
        {
            if (tileset.image_source_uri.empty())
            {
                prepared.tileset_textures.emplace_back();
                continue;
            }

            const std::optional<std::filesystem::path> base_path{
                resolve_tileset_base_path(working_record, tileset, vfs)
            };
            if (!base_path)
                return { .asset = { }, .error = tilemap_asset_load_error_t::resolve_failed };

            std::filesystem::path image_path{ normalize_tileset_image_path(tileset.image_source_uri) };
            if (!image_path.is_absolute())
                image_path = base_path->parent_path() / image_path;

            image_path = image_path.lexically_normal();

            if (!std::filesystem::exists(image_path))
            {
                LOG_ASSET_ERROR("Tilemap tileset image not found: raw='{}', resolved='{}'",
                                tileset.image_source_uri, image_path.string());
                return { .asset = { }, .error = tilemap_asset_load_error_t::source_not_found };
            }

            image_load_result_t image_result{ load_image_rgba8(image_path) };
            if (!image_result.success())
            {
                return { .asset = { }, .error = tilemap_asset_load_error_t::decode_failed };
            }

            prepared.tileset_textures.emplace_back(prepared_tilemap_tileset_texture_t{
                .width = image_result.image.width,
                .height = image_result.image.height,
                .stride_bytes = image_result.image.stride_bytes,
                .format = image_result.image.is_srgb ? rhi::texture_format_t::rgba8_srgb
                                                     : rhi::texture_format_t::rgba8_unorm,
                .pixel_payload = std::move(image_result.image.pixels)
            });
        }

        if (prepared.write_cooked_artifact)
        {
            cooked_tilemap_data_t cooked;
            cooked.importer_version = tilemap_importer_version;
            cooked.invalidation = expected_invalidation;
            cooked.tilemap = working_record.tilemap;

            if (!write_cooked_tilemap_file(cooked_path, cooked))
            {
                return { .asset = { }, .error = tilemap_asset_load_error_t::cooked_write_failed };
            }
        }

        return {
            .asset = std::move(prepared),
            .error = tilemap_asset_load_error_t::none
        };
    }

    tilemap_asset_load_result_t realize_prepared_tilemap_asset(prepared_tilemap_asset_t prepared,
                                                               rhi::rhi_context_t& rhi) noexcept
    {
        loaded_tilemap_asset_t loaded{ std::move(prepared.tilemap), prepared.record };
        for (prepared_tilemap_tileset_texture_t& prepared_texture : prepared.tileset_textures)
        {
            if (prepared_texture.width == 0u || prepared_texture.height == 0u)
            {
                loaded.add_tileset_texture(nullptr);
                continue;
            }

            rhi::texture_create_info_t texture_info{ };
            texture_info.width = prepared_texture.width;
            texture_info.height = prepared_texture.height;
            texture_info.format = prepared_texture.format;
            texture_info.initial_data = prepared_texture.pixel_payload.data();
            texture_info.initial_data_size = prepared_texture.pixel_payload.size();
            texture_info.initial_data_stride_bytes = prepared_texture.stride_bytes;

            std::unique_ptr<rhi::rhi_texture_t> texture{ rhi.create_texture_2d(texture_info) };
            if (!texture)
                return { .asset = { }, .error = tilemap_asset_load_error_t::texture_create_failed };

            loaded.add_tileset_texture(std::move(texture));
        }

        return {
            .asset = std::move(loaded),
            .error = tilemap_asset_load_error_t::none
        };
    }
} // namespace carrot::assets
