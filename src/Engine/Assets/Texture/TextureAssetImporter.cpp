//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TextureAssetImporter.h"

#include "Assets/Image/ImageAssetImporter.h"
#include "Utils/File/FileUtils.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] texture_import_error map_image_error(const image_load_error error) noexcept
        {
            switch (error)
            {
                case image_load_error::ok: return texture_import_error::ok;
                case image_load_error::file_not_found: return texture_import_error::file_not_found;
                case image_load_error::invalid_signature: return texture_import_error::invalid_signature;
                case image_load_error::file_too_short: return texture_import_error::file_too_short;
                case image_load_error::invalid_chunk_length: return texture_import_error::invalid_chunk_length;
                case image_load_error::crc_mismatch: return texture_import_error::crc_mismatch;
                case image_load_error::duplicate_ihdr: return texture_import_error::duplicate_ihdr;
                case image_load_error::missing_ihdr: return texture_import_error::missing_ihdr;
                case image_load_error::unexpected_chunk_order: return texture_import_error::unexpected_chunk_order;
                case image_load_error::unsupported_color_type: return texture_import_error::unsupported_color_type;
                case image_load_error::unsupported_bit_depth: return texture_import_error::unsupported_bit_depth;
                case image_load_error::no_idat_chunks: return texture_import_error::no_idat_chunks;
                case image_load_error::invalid_idat_stream: return texture_import_error::invalid_idat_stream;
                case image_load_error::no_iend: return texture_import_error::no_iend;
                case image_load_error::unsupported_compression_filter:
                    return texture_import_error::unsupported_compression_filter;
                case image_load_error::unsupported_interlace: return texture_import_error::unsupported_interlace;
                case image_load_error::unsupported_filter: return texture_import_error::unsupported_filter;
                default: return texture_import_error::unknown_error;
            }
        }
    } // anonymous namespace

    texture_import_result_t import_texture_asset(const std::string_view path) noexcept
    {
        texture_import_result_t result{ };

        image_load_result_t image_result{ load_image_rgba8(path) };
        if (!image_result.success())
        {
            result.error = map_image_error(image_result.error);
            return result;
        }

        auto image{ std::make_shared<image_rgba8_t>(std::move(image_result.image)) };
        result.texture = texture_asset_t{ std::move(image) };

        if (!result.texture.valid())
        {
            result.error = texture_import_error::invalid_source_image;
            return result;
        }

        return result;
    }
} // namespace carrot::assets
