//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/Audio/AudioAssetRegistry.h"
#include "Utils/JSON/Public/JsonDocument.h"

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    class audio_asset_manifest_importer_t
    {
    public:
        /**
         * @brief Import an audio asset from a JSON document.
         *
         * @param doc Parsed JSON document.
         * @param registry Audio asset registry to populate.
         * @return true on success, false on failure.
         */
        [[nodiscard]] static bool import(const utils::json::json_document_t& doc, audio_asset_registry_t& registry, const io::virtual_file_system_t& vfs);
    };
} // namespace carrot::assets
