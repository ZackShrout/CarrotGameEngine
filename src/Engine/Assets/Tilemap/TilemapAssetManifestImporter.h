//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "TilemapAssetRegistry.h"
#include "Utils/JSON/Public/JsonDocument.h"

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    class tilemap_asset_manifest_importer_t
    {
    public:
        [[nodiscard]] static bool import(const utils::json::json_document_t& doc,
                                         tilemap_asset_registry_t& registry,
                                         const io::virtual_file_system_t& vfs,
                                         std::string_view manifest_uri = {});
    };
} // namespace carrot::assets
