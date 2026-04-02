//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "IO/VirtualFileSystem.h"
#include "ShaderFileProvider.h"

#include <fstream>

namespace carrot::assets {
    class vfs_shader_file_provider_t final : public shader_file_provider_t
    {
    public:
        explicit vfs_shader_file_provider_t(io::virtual_file_system_t& vfs) : _vfs{ vfs } {}

        [[nodiscard]] std::optional<std::filesystem::path> resolve(const std::string_view virtual_path) const override
        {
            return _vfs.resolve_native_path(virtual_path);
        }

        [[nodiscard]] std::optional<std::vector<std::byte>> read(const std::string_view virtual_path) const override
        {
            const auto resolved{ _vfs.resolve_native_path(virtual_path) };
            if (!resolved)
                return std::nullopt;

            std::ifstream file(*resolved, std::ios::binary | std::ios::ate);
            if (!file)
                return std::nullopt;

            const auto size = file.tellg();
            if (size < 0)
                return std::nullopt;

            std::vector<std::byte> bytes(static_cast<size_t>(size));
            file.seekg(0, std::ios::beg);
            if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
                return std::nullopt;

            return bytes;
        }

    private:
        io::virtual_file_system_t& _vfs;
    };
} // namespace carrot::assets
