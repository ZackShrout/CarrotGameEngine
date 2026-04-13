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

    enum class imported_artifact_issue_t
    {
        none,
        missing_artifact,
        unreadable_artifact,
        importer_version_changed,
        source_changed,
        asset_definition_changed,
        import_settings_changed,
        reserved_changed,
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

    [[nodiscard]] imported_artifact_state_t inspect_imported_artifact_state(const imported_asset_invalidation_t& cached,
                                                                            const imported_asset_invalidation_t& expected,
                                                                            std::uint32_t cached_importer_version,
                                                                            std::uint32_t expected_importer_version,
                                                                            imported_artifact_issue_t& issue) noexcept;

    [[nodiscard]] constexpr std::string_view to_string(const imported_artifact_state_t state) noexcept
    {
        switch (state)
        {
            case imported_artifact_state_t::missing: return "missing";
            case imported_artifact_state_t::stale: return "stale";
            case imported_artifact_state_t::valid: return "valid";
            default: return "unknown";
        }
    }

    [[nodiscard]] constexpr std::string_view to_string(const imported_artifact_issue_t issue) noexcept
    {
        switch (issue)
        {
            case imported_artifact_issue_t::none: return "none";
            case imported_artifact_issue_t::missing_artifact: return "missing_artifact";
            case imported_artifact_issue_t::unreadable_artifact: return "unreadable_artifact";
            case imported_artifact_issue_t::importer_version_changed: return "importer_version_changed";
            case imported_artifact_issue_t::source_changed: return "source_changed";
            case imported_artifact_issue_t::asset_definition_changed: return "asset_definition_changed";
            case imported_artifact_issue_t::import_settings_changed: return "import_settings_changed";
            case imported_artifact_issue_t::reserved_changed: return "reserved_changed";
            default: return "unknown";
        }
    }
} // namespace carrot::assets
