//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <string>
#include <filesystem>
#include <optional>

namespace carrot::utils::file {
    std::optional<std::string> load_file_to_string(const std::filesystem::path& path);
} // namespace carrot::utils::file
