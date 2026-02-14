//
// Created by Zack Shrout on 2/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioAsset.h"
#include "Assets/AssetId.h"
#include "Assets/AssetHandle.h"
#include "Assets/AssetRegistry.h"

#include <vector>
#include <unordered_map>

namespace carrot::assets {
    using audio_asset_handle = asset_handle_t;

    class audio_asset_registry_t : public asset_registry_base_t
    {
    public:
        audio_asset_registry_t() = default;

        audio_asset_handle register_asset(asset_id_t id, const audio_asset_t& asset);

        [[nodiscard]] const audio_asset_t* get(audio_asset_handle handle) const noexcept;
        [[nodiscard]] audio_asset_handle find(asset_id_t id) const noexcept;
        [[nodiscard]] bool contains(asset_id_t id) const noexcept;
        [[nodiscard]] bool is_valid(audio_asset_handle handle) const noexcept;

        void clear();

    private:
        struct entry_t
        {
            audio_asset_t asset;
            uint32_t generation;
        };

        std::vector<entry_t> _assets;
        std::unordered_map<asset_id_t, uint32_t> _id_to_index;
    };
}
