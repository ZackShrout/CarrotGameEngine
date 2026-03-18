//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "FileUtils.h"

#include <fstream>

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

    std::string_view resolve_asset_path(const std::string_view path)
    {
        if (path.empty())
            return { };

        const std::filesystem::path p{ path };

        // NOTE: trusting an absolute path can be dangerous, but needed for tools
        if (p.is_absolute())
            return cache_path(std::string{ path });

        constexpr std::string_view res{ "res://" };
        constexpr std::string_view assets{ "assets://" };
        constexpr std::string_view content{ "content://" };
        constexpr std::string_view engine{ "engine://" };
        const std::filesystem::path content_root{ CARROT_SOURCE_ROOT };

        std::string_view prefixless{ path };

        if (path.starts_with(res))
        {
            prefixless = path.substr(res.size());
        }
        else if (path.starts_with(assets))
        {
            prefixless = path.substr(assets.size());
        }
        else if (path.starts_with(content))
        {
            prefixless = path.substr(content.size());
        }
        else if (path.starts_with(engine))
        {
            prefixless = path.substr(engine.size());
            return cache_path((content_root / "assets" / prefixless).string());
        }

        std::filesystem::path candidate{ content_root / prefixless };
        candidate = candidate.lexically_normal();

        return cache_path(candidate.string());
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
