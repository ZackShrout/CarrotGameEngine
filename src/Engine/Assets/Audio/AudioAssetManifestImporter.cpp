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

        const auto in_range = [](double v, double min, double max) noexcept
        {
            return v >= min && v <= max;
        };

        const auto is_integer = [](double v) noexcept
        {
            return std::floor(v) == v;
        };

        if (!id.data() || !source.data())
        {
            LOG_ASSET_ERROR("Missing required 'id' or 'file'");
            return false;
        }

        if (!is_valid_logical_asset_id(id))
        {
            const std::string suggested{ recommend_logical_asset_id(id) };

            if (!suggested.empty() && suggested != id)
                LOG_ASSET_ERROR(
                "Invalid logical asset id '{}'. Expected [a-z0-9._]+, no leading/trailing '.', and no consecutive dots. Consider '{}'.",
                id, suggested);
            else
                LOG_ASSET_ERROR(
                "Invalid logical asset id '{}'. Expected [a-z0-9._]+, no leading/trailing '.', and no consecutive dots.",
                id);

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

        const std::string_view bus_str{ root.get_string_or("bus", "sfx") };
        const std::string_view spatial_str{ root.get_string_or("spatial", "none") };
        const std::string_view distance_str{ root.get_string_or("distance", "none") };

        const audio::audio_bus_id bus_id{ audio::audio_bus_id_from_string(bus_str) };
        const audio::spatial_mode spatial_mode{ audio::spatial_mode_from_string(spatial_str) };
        const audio::distance_model distance_model{ audio::distance_model_from_string(distance_str) };

        if (bus_id == audio::audio_bus_id::unknown)
            LOG_ASSET_WARN("Unknown audio bus '{}' for asset '{}'; defaulting to 'sfx'", bus_str, id);

        if (spatial_mode == audio::spatial_mode::unknown)
            LOG_ASSET_WARN("Unknown spatial mode '{}' for asset '{}'; defaulting to 'none'", spatial_str, id);

        if (distance_model == audio::distance_model::unknown)
            LOG_ASSET_WARN("Unknown distance model '{}' for asset '{}'; defaulting to 'none'", distance_str, id);

        const double gain{ root.get_number_or("gain", 1.0) };
        const double pitch{ root.get_number_or("pitch", 1.0) };
        const double gain_variance{ root.get_number_or("gain_variance", 0.0) };
        const double pitch_variance{ root.get_number_or("pitch_variance", 0.0) };
        const double pan{ root.get_number_or("pan", 0.0) };
        const double min_distance{ root.get_number_or("min_distance", 1.0) };
        const double max_distance{ root.get_number_or("max_distance", 50.0) };
        const double max_voices{ root.get_number_or("max_voices", 0.0) };
        const double priority{ root.get_number_or("priority", 128.0) };

        if (gain < 0.0)
        {
            LOG_ASSET_ERROR("Audio asset '{}' has invalid gain {}. Expected >= 0.", id, gain);
            return false;
        }

        if (pitch <= 0.0)
        {
            LOG_ASSET_ERROR("Audio asset '{}' has invalid pitch {}. Expected > 0.", id, pitch);
            return false;
        }

        if (gain_variance < 0.0)
        {
            LOG_ASSET_ERROR("Audio asset '{}' has invalid gain_variance {}. Expected >= 0.", id, gain_variance);
            return false;
        }

        if (pitch_variance < 0.0)
        {
            LOG_ASSET_ERROR("Audio asset '{}' has invalid pitch_variance {}. Expected >= 0.", id, pitch_variance);
            return false;
        }

        if (pan < -1.0 || pan > 1.0)
        {
            LOG_ASSET_ERROR("Audio asset '{}' has invalid pan {}. Expected in [-1, 1].", id, pan);
            return false;
        }

        if (min_distance < 0.0)
        {
            LOG_ASSET_ERROR("Audio asset '{}' has invalid min_distance {}. Expected >= 0.", id, min_distance);
            return false;
        }

        if (max_distance < 0.0)
        {
            LOG_ASSET_ERROR("Audio asset '{}' has invalid max_distance {}. Expected >= 0.", id, max_distance);
            return false;
        }

        if (max_distance < min_distance)
        {
            LOG_ASSET_ERROR(
                "Audio asset '{}' has invalid distance range: min_distance {} > max_distance {}.",
                id, min_distance, max_distance
            );
            return false;
        }

        if (!is_integer(max_voices) || !in_range(max_voices, 0.0, 255.0))
        {
            LOG_ASSET_ERROR("Audio asset '{}' has invalid max_voices {}. Expected integer in [0, 255].", id, max_voices);
            return false;
        }

        if (!is_integer(priority) || !in_range(priority, 0.0, 255.0))
        {
            LOG_ASSET_ERROR("Audio asset '{}' has invalid priority {}. Expected integer in [0, 255].", id, priority);
            return false;
        }

        record.bus = bus_id == audio::audio_bus_id::unknown ? audio::audio_bus_id::sfx : bus_id;
        record.spatial = spatial_mode == audio::spatial_mode::unknown ? audio::spatial_mode::none : spatial_mode;
        record.distance = distance_model == audio::distance_model::unknown
                              ? audio::distance_model::none
                              : distance_model;

        record.gain = static_cast<float>(gain);
        record.pitch = static_cast<float>(pitch);
        record.gain_variance = static_cast<float>(gain_variance);
        record.pitch_variance = static_cast<float>(pitch_variance);
        record.pan = static_cast<float>(pan);
        record.min_distance = static_cast<float>(min_distance);
        record.max_distance = static_cast<float>(max_distance);
        record.max_voices = static_cast<uint8_t>(max_voices);
        record.priority = static_cast<uint8_t>(priority);

        record.streamed = root.get_bool_or("streamed", false);
        record.looping = root.get_bool_or("looping", false);

        if (record.looping)
        {
            const double loop_start{ root.get_number_or("loop_start", 0.0) };
            const double loop_end{ root.get_number_or("loop_end", 0.0) };

            if (!is_integer(loop_start) || loop_start < 0.0)
            {
                LOG_ASSET_ERROR("Audio asset '{}' has invalid loop_start {}. Expected integer >= 0.", id, loop_start);
                return false;
            }

            if (!is_integer(loop_end) || loop_end < 0.0)
            {
                LOG_ASSET_ERROR("Audio asset '{}' has invalid loop_end {}. Expected integer >= 0.", id, loop_end);
                return false;
            }

            if (loop_end != 0.0 && loop_end <= loop_start)
            {
                LOG_ASSET_ERROR(
                    "Audio asset '{}' has invalid loop range: loop_start {} and loop_end {}. Expected loop_end == 0 or > loop_start.",
                    id, loop_start, loop_end
                );
                return false;
            }

            record.loop_start = static_cast<uint32_t>(loop_start);
            record.loop_end = static_cast<uint32_t>(loop_end);
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
