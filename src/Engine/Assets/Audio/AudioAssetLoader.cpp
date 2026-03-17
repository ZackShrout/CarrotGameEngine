//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AudioAssetLoader.h"

#include "Audio/Sample/WavLoader.h"
#include "IO/VirtualFileSystem.h"

namespace carrot::assets {
    audio_asset_load_result_t load_audio_asset(const audio_asset_record_t& record,
        const io::virtual_file_system_t& vfs) noexcept
    {
        loaded_audio_asset_t loaded{ };
        loaded.record = &record;

        if (!record.streamed)
        {
            const auto native_path{ vfs.resolve_native_path(record.source_uri) };
            if (!native_path)
            {
                LOG_ASSET_ERROR("Failed to resolve audio source {}" , record.source_uri);

                return { { }, false };
            }

            loaded.sample = audio::load_wav_file(native_path->string());

            if (!loaded.sample)
            {
                LOG_ASSET_ERROR("Failed to load audio sample for asset '{}' from '{}'", record.logical_id,
                                record.source_uri);

                return { { }, false };
            }
        }

        return { loaded, true };
    }
} // namespace carrot::assets
