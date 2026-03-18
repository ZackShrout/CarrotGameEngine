//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "PlatformPaths.h"

#include <mach-o/dyld.h>
#include <vector>

namespace carrot::utils::file {
    [[nodiscard]] std::filesystem::path executable_path() noexcept
    {
        uint32_t size{ 0 };

        // First call to get buffer size
        _NSGetExecutablePath(nullptr, &size);

        std::vector<char> buffer(size);

        if (_NSGetExecutablePath(buffer.data(), &size) != 0) return { };

        const std::filesystem::path path{ buffer.data() };

        return std::filesystem::weakly_canonical(path);
    }
} // namespace carrot::utils::file
