//
// Created by Zack Shrout on 2/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "EngineConfig.h"

#include "Utils/JSON/Public/JsonDocument.h"

#include <span>

namespace carrot {
    namespace {
        using utils::json::json_enum_entry_t;

        constexpr json_enum_entry_t<rhi::graphics_api> graphics_api_map[]{
            { "direct_x12", rhi::graphics_api::direct_x12 },
            { "vulkan", rhi::graphics_api::vulkan },
            { "metal", rhi::graphics_api::metal },
        };

        using graphics_api_map_t = std::span<const json_enum_entry_t<rhi::graphics_api>>;
    }

    engine_config_t load_engine_config()
    {
        engine_config_t config{ };

        utils::json::json_document_t doc;
        if (!doc.parse_from_file("config/config.json"))
        {
            LOG_CORE_WARN("Using default engine configuration");
            config.graphics.api = rhi::graphics_api::default_api;
            config.graphics.enable_debug_layers = true;
            config.audio.sample_rate = 48000;
            config.audio.block_size = 512;
            config.audio.channels = 2;

            return config;
        }

        const utils::json::json_object_view_t root{ doc.root().as_object() };

        const int version{ static_cast<int>(root.get_number_or("version", 1)) };
        if (version != 1)
        {
            LOG_CORE_FATAL("Unsupported engine config version {} (expected 1)", version);
        }

        // ── 1. Graphics Configuration ─────────────────────────────────────────────

        const utils::json::json_object_view_t graphics{ root.require_object("graphics") };

        config.graphics.api = graphics.get_enum("api", graphics_api_map_t{ graphics_api_map },
                                                rhi::graphics_api::default_api);
        config.graphics.enable_debug_layers = graphics.get_bool_or("debug_layers", true);

        // ── 2. Audio Configuration ────────────────────────────────────────────────

        const utils::json::json_object_view_t audio{ root.require_object("audio") };

        config.audio.sample_rate = static_cast<uint32_t>(audio.get_number_or("sample_rate", 48000));
        config.audio.block_size = static_cast<uint32_t>(audio.get_number_or("block_size", 512));
        config.audio.channels = static_cast<uint32_t>(audio.get_number_or("channels", 2));

        return config;
    }
} // namespace carrot

