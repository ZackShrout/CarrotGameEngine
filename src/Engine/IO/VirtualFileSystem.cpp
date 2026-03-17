//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "VirtualFileSystem.h"

namespace carrot::io {
    // PUBLIC

    void virtual_file_system_t::mount(std::string scheme, std::filesystem::path root, const bool read_only)
    {
        if (scheme.empty())
            return;

        if (scheme.ends_with("://"))
            scheme.resize(scheme.size() - 3);

        vfs_mount_point_t& mount{ _mounts[scheme] };
        mount.scheme = scheme;
        mount.root = std::move(root);
        mount.read_only = read_only;
    }

    bool virtual_file_system_t::is_mounted(const std::string_view scheme) const noexcept
    {
        return _mounts.contains(std::string{ scheme });
    }

    bool virtual_file_system_t::is_virtual_path(const std::string_view path) const noexcept
    {
        return split_scheme(path).has_value();
    }

    std::optional<std::filesystem::path> virtual_file_system_t::resolve_native_path(
        const std::string_view virtual_path) const
    {
        if (virtual_path.empty())
            return std::nullopt;

        if (const std::filesystem::path fs_path{ virtual_path }; fs_path.is_absolute())
            return fs_path.lexically_normal();

        const auto split{ split_scheme(virtual_path) };
        if (!split)
            return std::nullopt;

        const auto& [scheme, remainder] = *split;

        const auto it{ _mounts.find(std::string{scheme}) };
        if (it == _mounts.end())
        {
            LOG_ASSET_ERROR("VFS scheme '{}' is not mounted", scheme);
            return std::nullopt;
        }

        const std::filesystem::path resolved{ it->second.root / remainder };
        if (!std::filesystem::exists(resolved))
        {
            LOG_ASSET_ERROR("Resolved VFS path '{}' to '{}', but the file does not exist", virtual_path,
                            resolved.string());
            return std::nullopt;
        }

        return resolved.lexically_normal();
    }

    bool virtual_file_system_t::exists(std::string_view path) const
    {
        const std::optional<std::filesystem::path> resolved{ resolve_native_path(path) };
        return resolved.has_value() && std::filesystem::exists(*resolved);
    }

    std::optional<vfs_mount_point_t> virtual_file_system_t::get_mount(const std::string_view scheme) const
    {
        if (const auto it{ _mounts.find(std::string{ scheme }) }; it != _mounts.end())
            return it->second;

        return std::nullopt;
    }

    // PRIVATE

    std::optional<std::pair<std::string_view, std::string_view>>
    virtual_file_system_t::split_scheme(std::string_view path) noexcept
    {
        const size_t pos{ path.find("://") };

        if (pos == std::string_view::npos || pos == 0)
            return std::nullopt;

        const std::string_view scheme{ path.substr(0, pos) };
        const std::string_view remainder{ path.substr(pos + 3) };

        return std::pair{ scheme, remainder };
    }
} // carrot::io
