//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "FileUtils.h"

namespace carrot::utils::file {
    namespace {
        std::unordered_map<std::string, std::string, std::hash<std::string>, std::equal_to<>> path_cache;
        std::mutex path_cache_mutex;

        std::string_view cache_path(std::string&& resolved)
        {
            std::lock_guard lock{ path_cache_mutex };
            auto [it, inserted]{ path_cache.try_emplace(std::move(resolved)) };

            return it->first;
        }
    } // anonymous namespace

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
        const auto utf8 = path.u8string();
        return std::string{ utf8.begin(), utf8.end() };
#else
        return path.string();
#endif
    }
} // namespace carrot::utils::file
