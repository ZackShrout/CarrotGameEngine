//
// Created by zshro on 3/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TextureAssetManifestImporter.h"

#include "Assets/AssetID.h"
#include "IO/VirtualFileSystem.h"

namespace carrot::assets {
    bool texture_asset_manifest_importer_t::import(
        const utils::json::json_document_t& doc,
        texture_asset_registry_t& registry,
        const io::virtual_file_system_t& vfs
    )
    {
        const utils::json::json_object_view_t root{ doc.root().as_object() };

        const std::string_view id{ root.get_string("id") };
        const std::string_view source{ root.get_string("source") };

        if (!id.data() || !source.data())
        {
            LOG_ASSET_ERROR("Missing required 'id' or 'source'");
            return false;
        }

        if (!is_valid_logical_asset_id(id))
        {
            const std::string suggested{ recommend_logical_asset_id(id) };

            if (!suggested.empty() && suggested != id)
            {
                LOG_ASSET_ERROR(
                    "Invalid logical asset id '{}'. Expected [a-z0-9._]+, no leading/trailing '.', and no consecutive dots. Consider '{}'.",
                    id, suggested
                );
            }
            else
            {
                LOG_ASSET_ERROR(
                    "Invalid logical asset id '{}'. Expected [a-z0-9._]+, no leading/trailing '.', and no consecutive dots.",
                    id
                );
            }

            return false;
        }

        texture_asset_record_t record{ };
        record.id = make_asset_id(id);
        record.logical_id = std::string{ id };

        if (registry.contains(record.id))
        {
            LOG_ASSET_ERROR("Duplicate texture asset id '{}'", id);
            return false;
        }

        record.source_uri = std::string{ source };

        if (!vfs.exists(record.source_uri))
        {
            LOG_ASSET_ERROR("Texture source not found '{}'", record.source_uri);
            return false;
        }

        record.srgb = root.get_bool_or("srgb", true);

        if (!registry.register_asset(std::move(record)))
        {
            LOG_ASSET_ERROR("Failed to register texture asset '{}'", id);
            return false;
        }

        LOG_ASSET_INFO("Registered texture asset '{}'", id);
        return true;
    }
} // namespace carrot::assets
