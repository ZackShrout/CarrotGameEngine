//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SpriteAssetLoader.h"

#include "Assets/ImportedAssetCache.h"
#include "Assets/Texture/TextureAssetSystem.h"
#include "CookedSprite.h"
#include "IO/VirtualFileSystem.h"
#include "Utils/File/FileUtils.h"

namespace carrot::assets {
    namespace {
        constexpr std::uint32_t sprite_importer_version{ 1u };

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

        [[nodiscard]] std::uint64_t compute_import_settings_hash(const sprite_asset_record_t& record) noexcept
        {
            std::uint64_t hash{ 14695981039346656037ull };
            hash_append_value(hash, record.schema_version);
            hash_append_string(hash, record.logical_id);
            hash_append_string(hash, record.source_uri);
            hash_append_string(hash, record.sprite.texture_id());
            hash_append_value(hash, record.sprite.default_pivot().x);
            hash_append_value(hash, record.sprite.default_pivot().y);
            hash_append_value(hash, record.sprite.pixels_per_unit());
            return hash;
        }

        [[nodiscard]] imported_asset_invalidation_t build_expected_invalidation(const sprite_asset_record_t& record,
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
    } // namespace

    std::filesystem::path cooked_sprite_cache_path(const std::string_view logical_id,
                                                   const io::virtual_file_system_t& vfs) noexcept
    {
        return imported_asset_cache_path(logical_id, "sprites", ".csprite", vfs);
    }

    sprite_asset_load_result_t load_sprite_asset(const sprite_asset_record_t& record,
                                                 const io::virtual_file_system_t& vfs,
                                                 texture_asset_system_t& textures)
    {
        if (record.id == 0 || record.logical_id.empty() || record.source_uri.empty())
        {
            return { .asset = { }, .error = sprite_asset_load_error_t::invalid_record };
        }

        const auto source_path{ vfs.resolve_native_path(record.source_uri) };
        if (!source_path || !std::filesystem::exists(*source_path))
        {
            return { .asset = { }, .error = sprite_asset_load_error_t::source_not_found };
        }

        if (!record.manifest_uri.empty())
        {
            const auto manifest_path{ vfs.resolve_native_path(record.manifest_uri) };
            if (!manifest_path || !std::filesystem::exists(*manifest_path))
                return { .asset = { }, .error = sprite_asset_load_error_t::manifest_not_found };
        }

        sprite_asset_t sprite{ record.sprite };
        bool loaded_from_cooked{ false };
        const imported_asset_invalidation_t expected_invalidation{ build_expected_invalidation(record, vfs) };
        const std::filesystem::path cooked_path{ cooked_sprite_cache_path(record.logical_id, vfs) };

        if (!cooked_path.empty() && std::filesystem::exists(cooked_path))
        {
            const auto cooked{ load_cooked_sprite_file(cooked_path) };
            if (cooked && is_imported_asset_current(cooked->invalidation,
                                                   expected_invalidation,
                                                   cooked->importer_version,
                                                   sprite_importer_version))
            {
                sprite = cooked->sprite;
                loaded_from_cooked = true;
                LOG_ASSET_INFO("Loaded sprite asset '{}' from cooked cache", record.logical_id);
            }
        }

        const loaded_texture_asset_t* texture{ textures.get(sprite.texture_id()) };
        if (!texture)
        {
            return {
                .asset = { },
                .error = sprite_asset_load_error_t::missing_texture_asset
            };
        }

        if (!loaded_from_cooked && !cooked_path.empty())
        {
            cooked_sprite_data_t cooked;
            cooked.importer_version = sprite_importer_version;
            cooked.invalidation = expected_invalidation;
            cooked.sprite = sprite;

            if (!write_cooked_sprite_file(cooked_path, cooked))
                return { .asset = { }, .error = sprite_asset_load_error_t::cooked_write_failed };
        }

        return {
            .asset = loaded_sprite_asset_t{ std::move(sprite), texture },
            .error = sprite_asset_load_error_t::none
        };
    }
} // namespace carrot::assets
