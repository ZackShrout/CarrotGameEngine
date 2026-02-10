//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "FileUtils.h"

#include <fstream>
#include <sstream>

namespace carrot::utils::file {
    std::optional<std::string> load_file_to_string(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::in | std::ios::binary);

        if (!file)
            return std::nullopt;

        file.seekg(0, std::ios::end);
        const std::streamsize size{ file.tellg() };

        if (size < 0)
            return std::nullopt;

        file.seekg(0, std::ios::beg);

        std::string buffer;
        buffer.resize(static_cast<size_t>(size));

        if (!file.read(buffer.data(), size))
            return std::nullopt;

        return buffer;
    }
} // namespace carrot::utils::file
