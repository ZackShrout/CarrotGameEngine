//
// Created by Zack Shrout on 2/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AssetService.h"

namespace carrot::assets {
    void asset_service_t::provide(asset_manager_t* manager) noexcept
    {
        if (_manager != nullptr) [[unlikely]]
        LOG_ASSET_WARN("Asset manager already provided");

        _manager = manager;
    }

    void asset_service_t::reset() noexcept
    {
        _manager = nullptr;
    }

    asset_manager_t& asset_service_t::manager()
    {
        if (_manager == nullptr) [[unlikely]]
        LOG_ASSET_FATAL("Asset manager not provided");

        return *_manager;
    }

    asset_manager_t* asset_service_t::try_manager() noexcept
    {
        return _manager;
    }

    // audio_asset_registry_t& asset_service_t::audio()
    // {
    //     if (_audio == nullptr) [[unlikely]]
    //     LOG_ASSET_FATAL("Audio asset registry not provided");
    //
    //     return *_audio;
    // }
    //
    // audio_asset_registry_t* asset_service_t::try_audio() noexcept
    // {
    //     return _audio;
    // }
} // namespace carrot::assets