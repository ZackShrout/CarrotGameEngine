//
// Created by Zack Shrout on 4/10/26.
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
    class font_asset_registry_t;

    class font_asset_manifest_importer_t
    {
    public:
        [[nodiscard]] static bool import(const utils::json::json_document_t& doc,
                                         font_asset_registry_t& registry,
                                         const io::virtual_file_system_t& vfs,
                                         std::string_view manifest_uri);
    };
} // namespace carrot::assets
