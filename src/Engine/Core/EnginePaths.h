//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <filesystem>
#include <optional>

namespace carrot::core {
    struct engine_paths_t
    {
        std::optional<std::filesystem::path> engine_root;
        std::optional<std::filesystem::path> engine_src;
        std::optional<std::filesystem::path> game_root;
        std::optional<std::filesystem::path> source_root;
        std::optional<std::filesystem::path> save_root;
    };
} // namespace carrot::core
