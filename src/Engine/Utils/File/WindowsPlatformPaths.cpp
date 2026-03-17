//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "PlatformPaths.h"

#include <Windows.h>

namespace carrot::utils::file {
    [[nodiscard]] std::filesystem::path executable_path() noexcept
    {
        std::vector<wchar_t> buffer(MAX_PATH);

        for (;;)
        {
            const DWORD copied{ GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size())) };

            if (copied == 0)
                return { };

            if (copied < buffer.size())
                return std::filesystem::path{ buffer.data(), buffer.data() + copied };

            buffer.resize(buffer.size() * 2);
        }
    }
} // namespace carrot::utils::file
