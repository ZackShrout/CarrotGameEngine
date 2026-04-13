//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetID.h"
#include "RHI/Texture.h"

#include <cstdint>
#include <memory>
#include <string>

namespace carrot::assets {
    struct texture_asset_record_t
    {
        asset_id_t id{ 0 };
        std::string logical_id;
        std::string source_uri;
        std::string manifest_uri;
        std::uint32_t schema_version{ 1u };
        bool srgb{ true };
    };

    struct loaded_texture_asset_t
    {
        const texture_asset_record_t* record{ nullptr };
        std::unique_ptr<rhi::rhi_texture_t> texture{ nullptr };

        [[nodiscard]] bool valid() const noexcept
        {
            return record != nullptr && texture != nullptr;
        }
    };
} // namespace carrot::assets
