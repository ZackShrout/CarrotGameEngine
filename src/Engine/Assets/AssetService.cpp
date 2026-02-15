//
// Created by Zack Shrout on 2/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AssetService.h"

namespace carrot::assets {
    void asset_service_t::provide(audio_asset_registry_t* audio) noexcept
    {
        if (_audio != nullptr) [[unlikely]]
        LOG_ASSET_WARN("Audio asset registry already provided");

        _audio = audio;
    }

    void asset_service_t::reset() noexcept
    {
        _audio = nullptr;
    }

    audio_asset_registry_t& asset_service_t::audio()
    {
        if (_audio == nullptr) [[unlikely]]
        LOG_ASSET_FATAL("Audio asset registry not provided");

        return *_audio;
    }

    audio_asset_registry_t* asset_service_t::try_audio() noexcept
    {
        return _audio;
    }
} // namespace carrot::assets