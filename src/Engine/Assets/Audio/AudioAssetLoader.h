//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioAsset.h"

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {

    enum class audio_asset_load_error
    {
        ok,
        source_not_found,
        resolve_failed,
        unsupported_format,
        decode_failed,
        invalid_record,
    };

    struct audio_asset_load_result_t
    {
        loaded_audio_asset_t asset;
        audio_asset_load_error error{ audio_asset_load_error::ok };

        [[nodiscard]] bool success() const noexcept
        {
            return error == audio_asset_load_error::ok;
        }
    };

    [[nodiscard]] audio_asset_load_result_t load_audio_asset(const audio_asset_record_t& record,
                                                             const io::virtual_file_system_t& vfs) noexcept;

    [[nodiscard]] constexpr std::string to_string(const audio_asset_load_error error) noexcept
    {
        switch (error)
        {
            case audio_asset_load_error::ok: return "ok";
            case audio_asset_load_error::source_not_found: return "source_not_found";
            case audio_asset_load_error::resolve_failed: return "resolve_failed";
            case audio_asset_load_error::unsupported_format: return "unsupported_format";
            case audio_asset_load_error::decode_failed: return "decode_failed";
            case audio_asset_load_error::invalid_record: return "invalid_record";
            default: return "unknown_error";
        }
    }
} // namespace carrot::assets
