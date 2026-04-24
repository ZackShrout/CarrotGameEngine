//
// Created by Zack Shrout on 4/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TiledWorld.h"

#include "IO/VirtualFileSystem.h"

#include <unordered_set>

namespace carrot::assets {
    namespace {
        void add_issue(std::vector<tilemap_validation_issue_t>& issues,
                       const tilemap_validation_issue_severity_t severity,
                       std::string code,
                       std::string message)
        {
            issues.push_back({
                .severity = severity,
                .code = std::move(code),
                .message = std::move(message)
            });
        }

        [[nodiscard]] std::optional<std::pair<std::string_view, std::string_view>>
        split_scheme(const std::string_view path) noexcept
        {
            const size_t pos{ path.find("://") };
            if (pos == std::string_view::npos || pos == 0u)
                return std::nullopt;

            return std::pair{ path.substr(0u, pos), path.substr(pos + 3u) };
        }

        [[nodiscard]] std::string resolve_relative_vfs_uri(const std::string_view base_uri,
                                                           const std::string_view relative_path)
        {
            const auto split{ split_scheme(base_uri) };
            if (!split)
                return std::string{ relative_path };

            const auto& [scheme, remainder] = *split;
            std::string normalized{ relative_path };
            std::replace(normalized.begin(), normalized.end(), '\\', '/');

            const std::filesystem::path base_path{ std::string{ remainder } };
            const std::filesystem::path resolved{
                (base_path.parent_path() / std::filesystem::path{ normalized }).lexically_normal()
            };

            return std::string{ scheme } + "://" + resolved.generic_string();
        }

        [[nodiscard]] bool vfs_exists_quietly(const io::virtual_file_system_t& vfs, const std::string_view uri)
        {
            if (uri.empty())
                return false;

            if (const std::filesystem::path fs_path{ std::string{ uri } }; fs_path.is_absolute())
                return std::filesystem::exists(fs_path);

            const auto split{ split_scheme(uri) };
            if (!split)
                return false;

            const auto& [scheme, remainder] = *split;
            const auto mount{ vfs.get_mount(scheme) };
            if (!mount)
                return false;

            return std::filesystem::exists((mount->root / remainder).lexically_normal());
        }

        [[nodiscard]] bool rectangles_overlap(const tiled_world_map_entry_t& a,
                                              const tiled_world_map_entry_t& b) noexcept
        {
            const int64_t a_left{ a.x };
            const int64_t a_top{ a.y };
            const int64_t a_right{ a_left + a.width };
            const int64_t a_bottom{ a_top + a.height };

            const int64_t b_left{ b.x };
            const int64_t b_top{ b.y };
            const int64_t b_right{ b_left + b.width };
            const int64_t b_bottom{ b_top + b.height };

            return a_left < b_right && a_right > b_left && a_top < b_bottom && a_bottom > b_top;
        }
    } // namespace

    std::optional<tiled_world_t> parse_tiled_world(const utils::json::json_document_t& doc,
                                                   const std::string_view source_uri,
                                                   const io::virtual_file_system_t* vfs)
    {
        if (!doc.valid() || !doc.root().is_object())
            return std::nullopt;

        const utils::json::json_object_view_t root{ doc.root().as_object() };

        tiled_world_t world;
        world.type = std::string{ root.get_string_or("type", "") };
        world.only_show_adjacent_maps = root.get_bool_or("onlyShowAdjacentMaps", false);
        world.source_uri = std::string{ source_uri };

        if (world.type.empty())
        {
            add_issue(world.validation_issues,
                      tilemap_validation_issue_severity_t::error,
                      "tiled.world.missing_type",
                      std::format("World '{}' is missing required top-level 'type'.",
                                  world.source_uri.empty() ? "<unknown>" : world.source_uri));
        }
        else if (world.type != "world")
        {
            add_issue(world.validation_issues,
                      tilemap_validation_issue_severity_t::error,
                      "tiled.world.invalid_type",
                      std::format("World '{}' declares type '{}', but Carrot expects type 'world'.",
                                  world.source_uri.empty() ? "<unknown>" : world.source_uri,
                                  world.type));
        }

        if (!root.has("maps") || !root.get("maps").is_array())
        {
            add_issue(world.validation_issues,
                      tilemap_validation_issue_severity_t::error,
                      "tiled.world.missing_maps",
                      std::format("World '{}' is missing required 'maps' array.",
                                  world.source_uri.empty() ? "<unknown>" : world.source_uri));
            return world;
        }

        const utils::json::json_array_view_t maps_json{ root.get_array("maps") };
        world.maps.reserve(maps_json.size());

        std::unordered_set<std::string> seen_map_uris;
        for (size_t i{ 0u }; i < maps_json.size(); ++i)
        {
            const auto map_value{ maps_json[i] };
            if (!map_value.is_object())
            {
                add_issue(world.validation_issues,
                          tilemap_validation_issue_severity_t::warning,
                          "tiled.world.map.invalid_entry",
                          std::format("World '{}' map entry {} is not an object and was ignored.",
                                      world.source_uri.empty() ? "<unknown>" : world.source_uri,
                                      i));
                continue;
            }

            const utils::json::json_object_view_t map_json{ map_value.as_object() };
            const std::string_view file_name{ map_json.get_string_or("fileName", "") };
            if (file_name.empty())
            {
                add_issue(world.validation_issues,
                          tilemap_validation_issue_severity_t::error,
                          "tiled.world.map.missing_file_name",
                          std::format("World '{}' map entry {} is missing non-empty 'fileName'.",
                                      world.source_uri.empty() ? "<unknown>" : world.source_uri,
                                      i));
                continue;
            }

            tiled_world_map_entry_t entry{
                .file_name = std::string{ file_name },
                .source_uri = resolve_relative_vfs_uri(source_uri, file_name),
                .x = static_cast<int32_t>(map_json.get_number_or("x", 0.0)),
                .y = static_cast<int32_t>(map_json.get_number_or("y", 0.0)),
                .width = static_cast<int32_t>(map_json.get_number_or("width", 0.0)),
                .height = static_cast<int32_t>(map_json.get_number_or("height", 0.0))
            };

            if (entry.width <= 0 || entry.height <= 0)
            {
                add_issue(world.validation_issues,
                          tilemap_validation_issue_severity_t::warning,
                          "tiled.world.map.non_positive_extent",
                          std::format("World '{}' map '{}' has non-positive extent {}x{}.",
                                      world.source_uri.empty() ? "<unknown>" : world.source_uri,
                                      entry.file_name,
                                      entry.width,
                                      entry.height));
            }

            if (!seen_map_uris.insert(entry.source_uri).second)
            {
                add_issue(world.validation_issues,
                          tilemap_validation_issue_severity_t::warning,
                          "tiled.world.map.duplicate_reference",
                          std::format("World '{}' references map '{}' more than once.",
                                      world.source_uri.empty() ? "<unknown>" : world.source_uri,
                                      entry.source_uri));
            }

            if (vfs && !vfs_exists_quietly(*vfs, entry.source_uri))
            {
                add_issue(world.validation_issues,
                          tilemap_validation_issue_severity_t::warning,
                          "tiled.world.map_reference_missing",
                          std::format("World '{}' references map '{}', but that file does not currently resolve.",
                                      world.source_uri.empty() ? "<unknown>" : world.source_uri,
                                      entry.source_uri));
            }

            for (const tiled_world_map_entry_t& existing : world.maps)
            {
                if (existing.width <= 0 || existing.height <= 0 || entry.width <= 0 || entry.height <= 0)
                    continue;

                if (rectangles_overlap(existing, entry))
                {
                    add_issue(world.validation_issues,
                              tilemap_validation_issue_severity_t::warning,
                              "tiled.world.map_bounds_overlap",
                              std::format("World '{}' places maps '{}' and '{}' on overlapping bounds.",
                                          world.source_uri.empty() ? "<unknown>" : world.source_uri,
                                          existing.file_name,
                                          entry.file_name));
                }
            }

            world.maps.emplace_back(std::move(entry));
        }

        return world;
    }
} // namespace carrot::assets
