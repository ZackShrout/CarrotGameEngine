//
// Created by Codex on 4/5/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "TilemapAsset.h"

#include <vector>

namespace carrot::assets {
    [[nodiscard]] std::vector<tilemap_validation_issue_t> validate_tiled_authored_data(const tilemap_asset_t& tilemap);
} // namespace carrot::assets
