//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "FileUtils.h"
#include "Utils/TextEncoding.h"

namespace carrot::utils::file {
    namespace {
    } // anonymous namespace

    FILE* open_file(const char* path, const char* mode) noexcept
    {
#ifdef _WIN32
        FILE* file{ nullptr };
        return (fopen_s(&file, path, mode) == 0) ? file : nullptr;
#else
        return std::fopen(path, mode);
#endif
    }

    int seek_file(FILE* file, const file_offset_t offset, const int origin) noexcept
    {
#ifdef _WIN32
        return _fseeki64(file, offset, origin);
#else
        return fseeko(file, offset, origin);
#endif
    }

    file_offset_t tell_file(FILE* file) noexcept
    {
#ifdef _WIN32
        return _ftelli64(file);
#else
        return ftello(file);
#endif
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> load_binary_file(const std::filesystem::path& path) noexcept
    {
        std::ifstream file{ path, std::ios::binary | std::ios::ate };
        if (!file.is_open())
        {
            LOG_CORE_ERROR("Failed to open file '{}'", to_log_string(path));
            return std::nullopt;
        }

        const std::streampos file_size{ file.tellg() };
        if (file_size < 0)
        {
            LOG_CORE_ERROR("Invalid file size for '{}'", to_log_string(path));
            return std::nullopt;
        }

        std::vector<std::uint8_t> data(static_cast<size_t>(file_size));

        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char *>(data.data()), file_size))
        {
            LOG_CORE_ERROR("Failed to read file '{}'", to_log_string(path));
            return std::nullopt;
        }

        return data;
    }

    std::optional<std::string> load_file_to_string(const std::filesystem::path& path) noexcept
    {
        auto bytes = load_binary_file(path);
        if (!bytes) return std::nullopt;

        return std::string(bytes->begin(), bytes->end());
    }

    std::string to_log_string(const std::filesystem::path& path)
    {
#ifdef _WIN32
        return utils::text::wide_to_utf8(path.native());
#else
        return path.string();
#endif
    }
} // namespace carrot::utils::file
