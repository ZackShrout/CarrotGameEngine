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
                return { { }, audio_asset_load_error::resolve_failed };

            loaded.sample.reset(audio::load_wav_file(native_path->string()));

            if (!loaded.sample)
                return { { }, audio_asset_load_error::decode_failed };
        }

        return { std::move(loaded), audio_asset_load_error::ok };
    }
} // namespace carrot::assets
