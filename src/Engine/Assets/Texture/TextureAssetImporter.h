//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "TextureAsset.h"

namespace carrot::assets {
    enum class texture_import_error : uint8_t
    {
        ok = 0,
        file_not_found,
        invalid_source_image,
        invalid_signature,
        file_too_short,
        invalid_chunk_length,
        crc_mismatch,
        duplicate_ihdr,
        missing_ihdr,
        unexpected_chunk_order,
        unsupported_color_type,
        unsupported_bit_depth,
        no_idat_chunks,
        invalid_idat_stream,
        no_iend,
        unsupported_compression_filter,
        unsupported_interlace,
        unsupported_filter,
        unknown_error,
    };

    struct texture_import_result_t
    {
        texture_asset_t texture;
        texture_import_error error{ texture_import_error::ok };

        [[nodiscard]] bool success() const noexcept
        {
            return error == texture_import_error::ok && texture.valid();
        }
    };

    [[nodiscard]] texture_import_result_t import_texture_asset(std::string_view path) noexcept;
} // namespace carrot::assets