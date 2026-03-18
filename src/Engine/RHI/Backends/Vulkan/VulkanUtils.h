//
// Created by zshrout on 3/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Utils/File/FileUtils.h"

namespace carrot::rhi::vulkan {
    [[nodiscard]] inline std::optional<std::vector<uint32_t>> load_spv_file(const std::filesystem::path& path) noexcept
    {
        const auto bytes{ utils::file::load_binary_file(path) };

        if (!bytes) return std::nullopt;

        if ((bytes->size() % sizeof(uint32_t)) != 0)
        {
            LOG_GRAPHICS_ERROR("SPIR-V file size not multiple of 4 bytes: {} bytes ({})",
                               bytes->size(), utils::file::to_log_string(path));
            return std::nullopt;
        }

        std::vector<uint32_t> words(bytes->size() / sizeof(uint32_t));
        std::memcpy(words.data(), bytes->data(), bytes->size());

        LOG_GRAPHICS_INFO("Loaded SPIR-V {}: {} words ({} bytes)",
                          utils::file::to_log_string(path), words.size(), bytes->size());

        return words;
    }
} // namespace carrot::rhi::vulkan