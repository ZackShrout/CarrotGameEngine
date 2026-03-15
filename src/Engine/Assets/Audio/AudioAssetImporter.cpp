//
// Created by Zack Shrout on 2/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AudioAssetImporter.h"

#include "Assets/AssetID.h"
#include "Audio/Sample/WavLoader.h"
#include "Utils/File/FileUtils.h"

namespace carrot::assets {
    bool audio_asset_importer_t::import(const utils::json::json_document_t& doc, audio_asset_registry_t& registry)
    {
        const utils::json::json_object_view_t root{ doc.root().as_object() };

        std::string_view id{ root.get_string("id") };
        std::string_view file{ root.get_string("file") };

        if (!id.data() || !file.data())
        {
            LOG_ASSET_ERROR("Missing required 'id' or 'file'");
            return false;
        }

        asset_id_t asset_id{ make_asset_id(id) };

        if (registry.contains(asset_id))
        {
            LOG_ASSET_ERROR("Duplicate audio asset id '{}'", id);
            return false;
        }

        // --------------------------------------------------
        // Resolve file path
        // --------------------------------------------------
        std::filesystem::path resolved_path{ utils::file::resolve_asset_path_fs(file) };

        if (!std::filesystem::exists(resolved_path))
        {
            LOG_ASSET_ERROR("Audio file not found '{}'", resolved_path.string());
            return false;
        }

        // --------------------------------------------------
        // Load or reference sample
        // --------------------------------------------------
        const bool streamed{ root.get_bool_or("streamed", false) };
        const audio::audio_sample_t* sample = nullptr;

        if (!streamed)
        {
            sample = audio::load_wav_file(resolved_path.string());

            if (!sample)
            {
                LOG_ASSET_ERROR("Failed to load sample '{}'", resolved_path.string());
                return false;
            }
        }

        // --------------------------------------------------
        // Construct asset
        // --------------------------------------------------

        audio_asset_t asset{ };
        asset.sample = sample;
        asset.streamed = streamed;
        asset.file_path = std::move(resolved_path);

        const audio::audio_bus_id bus_id{ audio::audio_bus_id_from_string(root.get_string_or("bus", "sfx")) };

        const audio::spatial_mode spatial_mode{
            audio::spatial_mode_from_string(root.get_string_or("spatial", "none"))
        };

        const audio::distance_model distance_model{
            audio::distance_model_from_string(root.get_string_or("distance", "none"))
        };

        asset.bus = bus_id == audio::audio_bus_id::unknown ? audio::audio_bus_id::sfx : bus_id;
        asset.gain = static_cast<float>(root.get_number_or("gain", 1.f));
        asset.pitch = static_cast<float>(root.get_number_or("pitch", 1.f));
        asset.gain_variance = static_cast<float>(root.get_number_or("gain_variance", 0.f));
        asset.pitch_variance = static_cast<float>(root.get_number_or("pitch_variance", 0.f));
        asset.spatial = spatial_mode == audio::spatial_mode::unknown ? audio::spatial_mode::none : spatial_mode;
        asset.pan = static_cast<float>(root.get_number_or("pan", 0.f));

        asset.distance = distance_model == audio::distance_model::unknown
                             ? audio::distance_model::none
                             : distance_model;

        asset.min_distance = static_cast<float>(root.get_number_or("min_distance", 1.0f));
        asset.max_distance = static_cast<float>(root.get_number_or("max_distance", 50.0f));
        asset.max_voices = static_cast<uint8_t>(root.get_number_or("max_voices", 0));
        asset.priority = static_cast<uint8_t>(root.get_number_or("priority", 128));

        // --------------------------------------------------
        // Looping
        // --------------------------------------------------

        asset.looping = root.get_bool_or("looping", false);

        if (asset.looping)
        {
            asset.loop_start = static_cast<uint32_t>(root.get_number_or("loop_start", 0));
            asset.loop_end = static_cast<uint32_t>(root.get_number_or("loop_end", 0));
        }

        // --------------------------------------------------
        // Register asset
        // --------------------------------------------------

        registry.register_asset(asset_id, std::move(asset));

        LOG_ASSET_INFO("Imported audio asset '{}'", id);
        return true;
    }
} // namespace carrot::assets
