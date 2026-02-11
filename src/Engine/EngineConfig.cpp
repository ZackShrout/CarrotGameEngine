//
// Created by Zack Shrout on 2/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "EngineConfig.h"

#include "Utils/Json/JsonDocument.h"

namespace carrot {
    namespace {
        rhi::graphics_api parse_graphics_api(std::string_view s)
        {
            if (s == "default") return rhi::graphics_api::default_api;
            if (s == "direct_x12")    return rhi::graphics_api::direct_x12;
            if (s == "vulkan")  return rhi::graphics_api::vulkan;
            if (s == "metal")   return rhi::graphics_api::metal;

            LOG_CORE_FATAL("Unknown graphics API '{}'", s);
            // return rhi::graphics_api::default_api;
            return rhi::graphics_api::direct_x12;
        }
    }

    engine_config_t load_engine_config()
    {
        engine_config_t config{};

        utils::json::json_document_t doc;
        if (!doc.parse_from_file("config/config.json"))
        {
            LOG_CORE_WARN("Using default engine configuration");
            config.graphics.api = rhi::graphics_api::default_api;
            config.graphics.enable_debug_layers = true;
            return config;
        }

        const utils::json::json_object_view_t root{ doc.root().as_object() };
        const utils::json::json_object_view_t graphics{ root.get_object("graphics") };

        // api
        const std::string_view api_str{ graphics.get_string_or("api", "default") };
        config.graphics.api = parse_graphics_api(api_str);

        // debug layers
        const utils::json::json_value_view_t debug_value{ graphics.get("debug_layers") };
        if (!debug_value || debug_value.as_string_or("") == "default")
        {
            config.graphics.enable_debug_layers = true;
        }
        else
        {
            config.graphics.enable_debug_layers = debug_value.as_bool();
        }

        return config;
    }

} // namespace carrot

