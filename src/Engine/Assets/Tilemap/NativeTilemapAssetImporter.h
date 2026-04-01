//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "TilemapAssetRegistry.h"
#include "Utils/JSON/Public/JsonDocument.h"

namespace carrot::assets {
    class native_tilemap_asset_importer_t
    {
    public:
        [[nodiscard]] static bool import(const utils::json::json_document_t& doc,
                                         tilemap_asset_registry_t& registry,
                                         std::string_view logical_id,
                                         std::string_view source_uri);
    };
} // namespace carrot::assets
