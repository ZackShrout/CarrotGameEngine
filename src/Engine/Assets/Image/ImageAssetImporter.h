//
// Created by zshrout on 3/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "ImageAsset.h"
#include "ImageLoadError.h"

#include <optional>
#include <string_view>

namespace carrot::assets {
    struct image_load_result_t
    {
        image_rgba8_t image;
        image_load_error error{ image_load_error::ok };

        [[nodiscard]] bool success() const noexcept
        {
            return error == image_load_error::ok;
        }
    };

    [[nodiscard]] image_load_result_t load_image_rgba8(std::string_view path) noexcept;
} // carrot::assets
