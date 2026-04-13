//
// Created by zshro on 3/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "TextureAssetRegistry.h"
#include "Utils/JSON/Public/JsonDocument.h"

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    class texture_asset_manifest_importer_t
    {
    public:
        [[nodiscard]] static bool import(
            const utils::json::json_document_t& doc,
            texture_asset_registry_t& registry,
            const io::virtual_file_system_t& vfs,
            std::string_view manifest_uri = {}
        );
    };
} // namespace carrot::assets
