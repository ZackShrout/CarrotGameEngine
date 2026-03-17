//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "PlatformPaths.h"

namespace carrot::utils::file {
    [[nodiscard]] std::filesystem::path executable_path()
    {
        return std::filesystem::path{ "" };
    }
} // namespace carrot::utils::file
