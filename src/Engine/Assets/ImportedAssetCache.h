//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    struct imported_asset_invalidation_t
    {
        std::uint64_t source_content_hash{ 0u };
        std::uint64_t asset_definition_content_hash{ 0u };
        std::uint64_t import_settings_hash{ 0u };
        std::uint64_t reserved_hash{ 0u };
    };

    enum class imported_artifact_state_t
    {
        missing,
        stale,
        valid,
    };

    [[nodiscard]] std::filesystem::path imported_asset_cache_path(std::string_view logical_id,
                                                                  std::string_view category,
                                                                  std::string_view extension,
                                                                  const io::virtual_file_system_t& vfs) noexcept;

    [[nodiscard]] std::optional<std::uint64_t> hash_vfs_file_contents(const io::virtual_file_system_t& vfs,
                                                                      std::string_view uri) noexcept;
    [[nodiscard]] std::uint64_t hash_bytes(std::span<const std::uint8_t> bytes) noexcept;
    void hash_append_bytes(std::uint64_t& hash, std::span<const std::uint8_t> bytes) noexcept;
    [[nodiscard]] bool is_imported_asset_current(const imported_asset_invalidation_t& cached,
                                                 const imported_asset_invalidation_t& expected,
                                                 std::uint32_t cached_importer_version,
                                                 std::uint32_t expected_importer_version) noexcept;
} // namespace carrot::assets
