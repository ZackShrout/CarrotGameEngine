//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace carrot::io {
    struct vfs_mount_point_t
    {
        std::string scheme;             // e.g. "engine", "game", "source", "save"
        std::filesystem::path root;     // real filesystem root
        bool read_only{ true };
    };

    class virtual_file_system_t
    {
    public:
        virtual_file_system_t() = default;

        void mount(std::string scheme, std::filesystem::path root, bool read_only = true);
        void unmount(const std::string_view scheme) { _mounts.erase(std::string{ scheme }); }
        void clear() { _mounts.clear(); }

        [[nodiscard]] bool is_mounted(std::string_view scheme) const noexcept;
        [[nodiscard]] std::optional<std::filesystem::path> resolve(std::string_view virtual_path) const;
        [[nodiscard]] bool is_virtual_path(std::string_view path) const noexcept;

        [[nodiscard]] std::optional<vfs_mount_point_t> get_mount(std::string_view scheme) const;

    private:
        [[nodiscard]] static std::optional<std::pair<std::string_view, std::string_view>>
        split_scheme(std::string_view path) noexcept;

        std::unordered_map<std::string, vfs_mount_point_t, std::hash<std::string>, std::equal_to<>> _mounts;
    };
} // carrot::io
