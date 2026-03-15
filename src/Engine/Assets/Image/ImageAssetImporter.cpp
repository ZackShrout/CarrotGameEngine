//
// Created by zshrout on 3/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "ImageAssetImporter.h"

#include "Utils/File/FileUtils.h"

#include <cpng/CarrotPNG.h>

namespace carrot::assets {
    namespace {
        [[nodiscard]] image_load_error map_decode_error(const cpng::decode_error err) noexcept
        {
            switch (err)
            {
                case cpng::decode_error::ok: return image_load_error::ok;
                case cpng::decode_error::file_not_found: return image_load_error::file_not_found;
                case cpng::decode_error::invalid_signature: return image_load_error::invalid_signature;
                case cpng::decode_error::file_too_short: return image_load_error::file_too_short;
                case cpng::decode_error::invalid_chunk_length: return image_load_error::invalid_chunk_length;
                case cpng::decode_error::crc_mismatch: return image_load_error::crc_mismatch;
                case cpng::decode_error::duplicate_ihdr: return image_load_error::duplicate_ihdr;
                case cpng::decode_error::missing_ihdr: return image_load_error::missing_ihdr;
                case cpng::decode_error::unexpected_chunk_order: return image_load_error::unexpected_chunk_order;
                case cpng::decode_error::unsupported_color_type: return image_load_error::unsupported_color_type;
                case cpng::decode_error::unsupported_bit_depth: return image_load_error::unsupported_bit_depth;
                case cpng::decode_error::no_idat_chunks: return image_load_error::no_idat_chunks;
                case cpng::decode_error::invalid_idat_stream: return image_load_error::invalid_idat_stream;
                case cpng::decode_error::no_iend: return image_load_error::no_iend;
                case cpng::decode_error::unsupported_compression_filter:
                    return image_load_error::unsupported_compression_filter;
                case cpng::decode_error::unsupported_interlace: return image_load_error::unsupported_interlace;
                case cpng::decode_error::unsupported_filter: return image_load_error::unsupported_filter;
                default: return image_load_error::unknown_error;
            }
        }
    } // anonymous namespace

    image_load_result_t load_image_rgba8(const std::string_view path) noexcept
    {
        image_load_result_t result{ };
        cpng::image_view_t decoded_view{ };
        std::vector<uint8_t> decoded_pixels{ };

        const std::string path_string{ utils::file::resolve_asset_path(path) };
        const cpng::decode_error err{ cpng::load_from_file(path_string.c_str(), decoded_view, decoded_pixels) };

        result.error = map_decode_error(err);
        if (result.error != image_load_error::ok)
            return result;

        result.image.width = decoded_view.width;
        result.image.height = decoded_view.height;
        result.image.stride_bytes = decoded_view.stride_bytes;
        result.image.is_srgb = decoded_view.is_srgb;
        result.image.pixels = std::move(decoded_pixels);

        return result;
    }
} // carrot::assets
