//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "ImportedAssetCache.h"

#include "IO/VirtualFileSystem.h"
#include "Utils/File/FileUtils.h"

namespace carrot::assets {
    namespace {
        constexpr std::uint64_t fnv_offset{ 14695981039346656037ull };
        constexpr std::uint64_t fnv_prime{ 1099511628211ull };
    }

    std::filesystem::path imported_asset_cache_path(const std::string_view logical_id,
                                                    const std::string_view category,
                                                    const std::string_view extension,
                                                    const io::virtual_file_system_t& vfs) noexcept
    {
        const auto save_mount{ vfs.get_mount("save") };
        if (!save_mount)
            return {};

        return save_mount->root / "cache" / std::string{ category } / (std::string{ logical_id } + std::string{ extension });
    }

    std::optional<std::uint64_t> hash_vfs_file_contents(const io::virtual_file_system_t& vfs,
                                                        const std::string_view uri) noexcept
    {
        const auto path{ vfs.resolve_native_path(uri) };
        if (!path)
            return std::nullopt;

        const auto bytes{ utils::file::load_binary_file(*path) };
        if (!bytes)
            return std::nullopt;

        return hash_bytes(*bytes);
    }

    std::uint64_t hash_bytes(const std::span<const std::uint8_t> bytes) noexcept
    {
        std::uint64_t hash{ fnv_offset };
        hash_append_bytes(hash, bytes);
        return hash;
    }

    void hash_append_bytes(std::uint64_t& hash, const std::span<const std::uint8_t> bytes) noexcept
    {
        for (const std::uint8_t byte : bytes)
        {
            hash ^= static_cast<std::uint64_t>(byte);
            hash *= fnv_prime;
        }
    }

    bool is_imported_asset_current(const imported_asset_invalidation_t& cached,
                                   const imported_asset_invalidation_t& expected,
                                   const std::uint32_t cached_importer_version,
                                   const std::uint32_t expected_importer_version) noexcept
    {
        return cached_importer_version == expected_importer_version &&
               cached.source_content_hash == expected.source_content_hash &&
               cached.asset_definition_content_hash == expected.asset_definition_content_hash &&
               cached.import_settings_hash == expected.import_settings_hash &&
               cached.reserved_hash == expected.reserved_hash;
    }
} // namespace carrot::assets
