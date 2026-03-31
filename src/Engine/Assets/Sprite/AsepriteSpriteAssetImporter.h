//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "SpriteAssetRegistry.h"
#include "Utils/JSON/Public/JsonDocument.h"

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    class aseprite_sprite_asset_importer_t
    {
    public:
        [[nodiscard]] static bool import(const utils::json::json_document_t& doc, sprite_asset_registry_t& registry,
                                         std::string_view logical_id, std::string_view texture_id);
    };
} // namespace carrot::assets
