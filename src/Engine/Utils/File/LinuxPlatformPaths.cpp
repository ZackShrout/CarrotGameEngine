//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "PlatformPaths.h"

#include <unistd.h>

namespace carrot::utils::file {
    [[nodiscard]] std::filesystem::path executable_path() noexcept
    {
        std::vector<char> buffer(1024);

        for (;;)
        {
            const ssize_t count{ readlink("/proc/self/exe", buffer.data(), buffer.size()) };

            if (count == -1)
            {
                return { };
            }

            if (static_cast<size_t>(count) < buffer.size())
            {
                return std::filesystem::weakly_canonical(std::filesystem::path{
                    buffer.data(), buffer.data() + count
                });
            }

            buffer.resize(buffer.size() * 2);
        }
    }
} // namespace carrot::utils::file
