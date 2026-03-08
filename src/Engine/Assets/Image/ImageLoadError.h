//
// Created by zshrout on 3/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <string_view>

namespace carrot::assets {
    enum class image_load_error : unsigned char
    {
        ok = 0,
        file_not_found,
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

    [[nodiscard]] std::string_view to_string(image_load_error err) noexcept;
} // carrot::assets
