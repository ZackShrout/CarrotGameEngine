//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#include "ShaderUtils.h"

#include "Engine/Common/CommonHeaders.h"

#include <fstream>

std::vector<uint32_t> load_spv(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        LOG_GRAPHICS_ERROR("Failed to open SPV: " + path);
        return { };
    }

    const size_t size{ static_cast<size_t>(file.tellg()) };
    file.seekg(0);
    std::vector<uint32_t> buffer((size + 3) / 4);
    file.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(size));
    file.close();
    return buffer;
}
