//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AssetManager.h"

namespace carrot::assets {
    void asset_manager_t::clear()
    {
        _audio.clear_all();
        _textures.clear_all();
        _sprites.clear_all();
        _tilemaps.clear_all();
        _scenes.clear_all();
    }
} // namespace carrot::assets
