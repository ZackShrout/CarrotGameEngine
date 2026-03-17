//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioAsset.h"

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    struct audio_asset_load_result_t
    {
        loaded_audio_asset_t asset;
        bool success{ false };
    };

    [[nodiscard]] audio_asset_load_result_t load_audio_asset(const audio_asset_record_t& record,
                                                             const io::virtual_file_system_t& vfs) noexcept;
} // namespace carrot::assets
