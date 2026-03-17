//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <filesystem>

namespace carrot::utils::file {
    [[nodiscard]] std::filesystem::path executable_path() noexcept;

    [[nodiscard]] inline std::filesystem::path executable_directory() noexcept
    {
        return executable_path().parent_path();
    }
} // namespace carrot::utils::file
