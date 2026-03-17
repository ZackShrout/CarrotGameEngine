//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AudioAssetManifestImporter.h"

#include "Assets/AssetID.h"
#include "IO/VirtualFileSystem.h"

namespace carrot::assets {
    bool audio_asset_manifest_importer_t::import(const utils::json::json_document_t& doc,
        audio_asset_registry_t& registry, const io::virtual_file_system_t& vfs)
    {
        const utils::json::json_object_view_t root{ doc.root().as_object() };

        const std::string_view id{ root.get_string("id") };
        const std::string_view source{ root.get_string("source") };

        if (!id.data() || !source.data())
        {
            LOG_ASSET_ERROR("Missing required 'id' or 'file'");
            return false;
        }

        audio_asset_record_t record{ };
        record.id = make_asset_id(id);
        record.logical_id = std::string{ id };

        if (registry.contains(record.id))
        {
            LOG_ASSET_ERROR("Duplicate audio asset id '{}'", id);
            return false;
        }

        record.source_uri = std::string{ source };

        if (!vfs.exists(record.source_uri))
        {
            LOG_ASSET_ERROR("Audio source not found '{}'", record.source_uri);
            return false;
        }

        const audio::audio_bus_id bus_id{ audio::audio_bus_id_from_string(root.get_string_or("bus", "sfx")) };

        const audio::spatial_mode spatial_mode{
            audio::spatial_mode_from_string(root.get_string_or("spatial", "none"))
        };

        const audio::distance_model distance_model{
            audio::distance_model_from_string(root.get_string_or("distance", "none"))
        };

        record.bus = bus_id == audio::audio_bus_id::unknown ? audio::audio_bus_id::sfx : bus_id;
        record.gain = static_cast<float>(root.get_number_or("gain", 1.0));
        record.pitch = static_cast<float>(root.get_number_or("pitch", 1.0));
        record.gain_variance = static_cast<float>(root.get_number_or("gain_variance", 0.0));
        record.pitch_variance = static_cast<float>(root.get_number_or("pitch_variance", 0.0));

        record.streamed = root.get_bool_or("streamed", false);

        record.spatial = spatial_mode == audio::spatial_mode::unknown ? audio::spatial_mode::none : spatial_mode;

        record.pan = static_cast<float>(root.get_number_or("pan", 0.0));

        record.distance = distance_model == audio::distance_model::unknown
                              ? audio::distance_model::none
                              : distance_model;

        record.min_distance = static_cast<float>(root.get_number_or("min_distance", 1.0));
        record.max_distance = static_cast<float>(root.get_number_or("max_distance", 50.0));

        record.max_voices = static_cast<uint8_t>(root.get_number_or("max_voices", 0));
        record.priority = static_cast<uint8_t>(root.get_number_or("priority", 128));

        record.looping = root.get_bool_or("looping", false);

        if (record.looping)
        {
            record.loop_start = static_cast<uint32_t>(root.get_number_or("loop_start", 0));
            record.loop_end = static_cast<uint32_t>(root.get_number_or("loop_end", 0));
        }

        if (!registry.register_asset(std::move(record)))
        {
            LOG_ASSET_ERROR("Failed to register audio asset '{}'", id);
            return false;
        }

        LOG_ASSET_INFO("Registered audio asset '{}'", id);
        return true;
    }
} // namespace carrot::assets
