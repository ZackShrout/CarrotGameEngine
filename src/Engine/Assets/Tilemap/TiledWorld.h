//
// Created by Zack Shrout on 4/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "TilemapAsset.h"
#include "Utils/JSON/Public/JsonDocument.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    struct tiled_world_map_entry_t
    {
        std::string file_name;
        std::string source_uri;
        int32_t x{ 0 };
        int32_t y{ 0 };
        int32_t width{ 0 };
        int32_t height{ 0 };
    };

    struct tiled_world_t
    {
        std::string type;
        bool only_show_adjacent_maps{ false };
        std::string source_uri;
        std::vector<tiled_world_map_entry_t> maps;
        std::vector<tilemap_validation_issue_t> validation_issues;
    };

    [[nodiscard]] std::optional<tiled_world_t> parse_tiled_world(
        const utils::json::json_document_t& doc,
        std::string_view source_uri,
        const io::virtual_file_system_t* vfs = nullptr);
} // namespace carrot::assets
