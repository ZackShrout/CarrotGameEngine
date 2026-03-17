//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace carrot::assets {
    class shader_file_provider_t
    {
    public:
        virtual ~shader_file_provider_t() = default;

        [[nodiscard]] virtual std::optional<std::filesystem::path> resolve(std::string_view virtual_path) const = 0;
        [[nodiscard]] virtual std::optional<std::vector<std::byte>> read(std::string_view virtual_path) const = 0;
    };
} // namespace carrot::assets