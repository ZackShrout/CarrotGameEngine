//
// Created by Codex on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::utils::json {
    class json_document_t;
}

namespace carrot::assets {
    class scene_asset_registry_t;

    class scene_asset_manifest_importer_t
    {
    public:
        [[nodiscard]] static bool import(const utils::json::json_document_t& doc,
                                         scene_asset_registry_t& registry,
                                         const io::virtual_file_system_t& vfs,
                                         std::string_view manifest_uri = {});
    };
} // namespace carrot::assets
