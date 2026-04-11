//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "FontAssetManifestImporter.h"

#include "Assets/AssetID.h"
#include "FontAssetRegistry.h"
#include "IO/VirtualFileSystem.h"
#include "Utils/JSON/Public/JsonDocument.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] std::optional<font_charset_preset_t> parse_charset_preset(const std::string_view preset)
        {
            if (preset == "BasicLatin")
                return font_charset_preset_t::basic_latin;

            return std::nullopt;
        }

        [[nodiscard]] bool read_codepoint_list(const utils::json::json_object_view_t& object,
                                               const char* key,
                                               std::vector<std::uint32_t>& out_values)
        {
            if (!object.has(key))
                return true;

            out_values.clear();
            for (const utils::json::json_value_view_t value : object.get_array(key))
            {
                if (!value.is_number())
                {
                    LOG_ASSET_ERROR("Font asset '{}' must contain only numeric codepoints", key);
                    return false;
                }

                const double number{ value.as_number() };
                if (number < 0.0 || number > static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
                {
                    LOG_ASSET_ERROR("Font asset '{}' includes out-of-range codepoint {}", key, number);
                    return false;
                }

                out_values.push_back(static_cast<std::uint32_t>(number));
            }

            std::sort(out_values.begin(), out_values.end());
            out_values.erase(std::unique(out_values.begin(), out_values.end()), out_values.end());
            return true;
        }
    } // namespace

    bool font_asset_manifest_importer_t::import(const utils::json::json_document_t& doc,
                                                font_asset_registry_t& registry,
                                                const io::virtual_file_system_t& vfs,
                                                const std::string_view manifest_uri)
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
                    id,
                    suggested
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

        font_asset_record_t record{ };
        record.id = make_asset_id(id);
        record.logical_id = std::string{ id };
        record.source_uri = std::string{ source };
        record.manifest_uri = std::string{ manifest_uri };
        record.schema_version = static_cast<std::uint32_t>(root.get_number_or("version", 1.0));

        if (record.schema_version != 1u)
        {
            LOG_ASSET_ERROR("Font asset '{}' uses unsupported schema version {}", id, record.schema_version);
            return false;
        }

        if (registry.contains(record.id))
        {
            LOG_ASSET_ERROR("Duplicate font asset id '{}'", id);
            return false;
        }

        if (!vfs.exists(record.source_uri))
        {
            LOG_ASSET_ERROR("Font source not found '{}'", record.source_uri);
            return false;
        }

        if (!root.has("charset") || !root.has("msdf"))
        {
            LOG_ASSET_ERROR("Font asset '{}' is missing required 'charset' or 'msdf' object", id);
            return false;
        }

        const utils::json::json_object_view_t charset{ root.get_object("charset") };
        const std::string_view preset_str{ charset.get_string("preset") };
        const auto preset{ parse_charset_preset(preset_str) };
        if (!preset.has_value())
        {
            LOG_ASSET_ERROR("Font asset '{}' uses unsupported charset preset '{}'", id, preset_str);
            return false;
        }
        record.charset_preset = *preset;

        if (!read_codepoint_list(charset, "include_codepoints", record.include_codepoints) ||
            !read_codepoint_list(charset, "exclude_codepoints", record.exclude_codepoints))
        {
            LOG_ASSET_ERROR("Font asset '{}' contains invalid codepoint list data", id);
            return false;
        }

        const utils::json::json_object_view_t msdf{ root.get_object("msdf") };
        record.msdf.atlas_width = static_cast<std::uint32_t>(msdf.get_number("atlas_width"));
        record.msdf.atlas_height = static_cast<std::uint32_t>(msdf.get_number("atlas_height"));
        record.msdf.pixel_range = static_cast<float>(msdf.get_number("pixel_range"));
        record.msdf.include_kerning = msdf.get_bool_or("include_kerning", true);

        if (record.msdf.atlas_width == 0u || record.msdf.atlas_height == 0u)
        {
            LOG_ASSET_ERROR("Font asset '{}' must use positive atlas dimensions", id);
            return false;
        }

        if (record.msdf.pixel_range <= 0.0f)
        {
            LOG_ASSET_ERROR("Font asset '{}' must use a positive msdf.pixel_range", id);
            return false;
        }

        if (root.has("defaults"))
        {
            const utils::json::json_object_view_t defaults{ root.get_object("defaults") };
            record.defaults.line_height_scale = static_cast<float>(defaults.get_number_or("line_height_scale", 1.0));
            if (record.defaults.line_height_scale <= 0.0f)
            {
                LOG_ASSET_ERROR("Font asset '{}' must use a positive defaults.line_height_scale", id);
                return false;
            }
        }

        if (!registry.register_asset(std::move(record)))
        {
            LOG_ASSET_ERROR("Failed to register font asset '{}'", id);
            return false;
        }

        LOG_ASSET_INFO("Registered font asset '{}'", id);
        return true;
    }
} // namespace carrot::assets
