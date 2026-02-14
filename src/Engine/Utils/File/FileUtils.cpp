//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "FileUtils.h"

#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>

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

    std::optional<std::string> load_file_to_string(const std::filesystem::path& path)
    {
        std::ifstream file{ path, std::ios::in | std::ios::binary };

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
            return cache_path((content_root/ "Engine" / prefixless).string());
        }

        std::filesystem::path candidate{ content_root / prefixless };
        candidate = candidate.lexically_normal();

        return cache_path(candidate.string());
    }

    std::optional<std::filesystem::path> try_resolve_asset_path(const std::string_view path)
    {
        if (path.empty())
            return std::nullopt;

        const std::filesystem::path p{ path };
        const std::filesystem::path content_root{ CARROT_SOURCE_ROOT };

        if (p.is_absolute())
            return p.lexically_normal();

        std::string_view prefixless{ path };

        constexpr std::string_view prefixes[]{ "res://", "assets://", "content://" };

        for (const auto prefix: prefixes)
        {
            if (path.starts_with(prefix))
            {
                prefixless = path.substr(prefix.size());
                break;
            }
        }

        std::filesystem::path resolved{ (content_root / prefixless).lexically_normal() };

        return resolved;
    }
} // namespace carrot::utils::file
