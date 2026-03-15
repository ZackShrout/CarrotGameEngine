//
// Created by zshrout on 3/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "ImageLoadError.h"

namespace carrot::assets {
    std::string_view to_string(const image_load_error err) noexcept
    {
        switch (err)
        {
            case image_load_error::ok: return "ok";
            case image_load_error::file_not_found: return "file not found";
            case image_load_error::invalid_signature: return "invalid signature";
            case image_load_error::file_too_short: return "file too short";
            case image_load_error::invalid_chunk_length: return "invalid chunk length";
            case image_load_error::crc_mismatch: return "crc mismatch";
            case image_load_error::duplicate_ihdr: return "duplicate IHDR";
            case image_load_error::missing_ihdr: return "missing IHDR";
            case image_load_error::unexpected_chunk_order: return "unexpected chunk order";
            case image_load_error::unsupported_color_type: return "unsupported color type";
            case image_load_error::unsupported_bit_depth: return "unsupported bit depth";
            case image_load_error::no_idat_chunks: return "no IDAT chunks";
            case image_load_error::invalid_idat_stream: return "invalid IDAT stream";
            case image_load_error::no_iend: return "missing IEND";
            case image_load_error::unsupported_compression_filter: return "unsupported compression/filter";
            case image_load_error::unsupported_interlace: return "unsupported interlace";
            case image_load_error::unsupported_filter: return "unsupported filter";
            case image_load_error::unknown_error: return "unknown error";
            default: return "unknown error";
        }
    }
} // carrot::assets
