//
// Created by Zack Shrout on 2/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioAssetRegistry.h"
#include "Utils/JSON/Public/JsonDocument.h"

namespace carrot::assets {
    class audio_asset_importer_t
    {
    public:
        /**
         * @brief Import an audio asset from a JSON document.
         *
         * @param doc Parsed JSON document.
         * @param registry Audio asset registry to populate.
         * @return true on success, false on failure.
         */
        static bool import(const utils::json::json_document_t& doc, audio_asset_registry_t& registry);
    };
} // namespace carrot::assets