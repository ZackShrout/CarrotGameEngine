//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TilemapAssetManifestImporter.h"

#include "Assets/AssetID.h"
#include "IO/VirtualFileSystem.h"
#include "NativeTilemapAssetImporter.h"
#include "TiledTilemapAssetImporter.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] bool ends_with(const std::string_view value, const std::string_view suffix) noexcept
        {
            return value.size() >= suffix.size() &&
                   value.substr(value.size() - suffix.size()) == suffix;
        }
    }

    bool tilemap_asset_manifest_importer_t::import(const utils::json::json_document_t& doc,
                                                   tilemap_asset_registry_t& registry,
                                                   const io::virtual_file_system_t& vfs)
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

        if (registry.contains(make_asset_id(id)))
        {
            LOG_ASSET_ERROR("Duplicate tilemap asset id '{}'", id);
            return false;
        }

        if (!vfs.exists(source))
        {
            LOG_ASSET_ERROR("Tilemap source not found '{}'", source);
            return false;
        }

        const std::optional<std::filesystem::path> native_path{ vfs.resolve_native_path(source) };
        if (!native_path)
        {
            LOG_ASSET_ERROR("Failed to resolve tilemap source '{}'", source);
            return false;
        }

        utils::json::json_document_t imported_doc;
        if (!imported_doc.parse_from_file(native_path->string().c_str()))
        {
            LOG_ASSET_ERROR("Failed to parse tilemap source '{}'", source);
            return false;
        }

        if (ends_with(source, ".tmj"))
            return tiled_tilemap_asset_importer_t::import(imported_doc, registry, id, source);

        if (ends_with(source, ".ctilemap.json"))
            return native_tilemap_asset_importer_t::import(imported_doc, registry, id, source);

        LOG_ASSET_ERROR("Unsupported tilemap source format '{}'", source);
        return false;
    }
} // namespace carrot::assets
