//
// Created by zshrout on 12/31/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#pragma once

#include <cstdint>
#include <format>
#include <mutex>
#include <print>
#include <source_location>
#include <string>

namespace carrot::core {
    struct log_message;
    class log_sink_t;

    enum class log_category : uint32_t
    {
        core = 1 << 0,
        graphics = 1 << 1,
        audio = 1 << 2,
        physics = 1 << 3,
        input = 1 << 4,
        network = 1 << 5,
        ui = 1 << 6,
        asset = 1 << 7,
        script = 1 << 8,
        // ... add more as needed

        all = ~0u, // All bits set
    };

    enum class log_severity : uint8_t
    {
        trace,
        debug,
        info,
        warn,
        error,
        fatal
    };

    constexpr log_category operator|(log_category a, log_category b) noexcept
    {
        return static_cast<log_category>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
        );
    }

    constexpr log_category operator&(log_category a, log_category b) noexcept
    {
        return static_cast<log_category>(
            static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
        );
    }

    constexpr log_category operator~(log_category a) noexcept
    {
        return static_cast<log_category>(
            ~static_cast<uint32_t>(a)
        );
    }

    constexpr bool operator==(log_category a, uint32_t b) noexcept
    {
        return static_cast<uint32_t>(a) == b;
    }

    constexpr bool operator==(log_category a, log_category b) noexcept
    {
        return static_cast<uint32_t>(a) == static_cast<uint32_t>(b);
    }

    constexpr log_category& operator|=(log_category& a, log_category b) noexcept
    {
        return a = a | b;
    }

    constexpr log_category& operator&=(log_category& a, log_category b) noexcept
    {
        return a = a & b;
    }

    constexpr bool any(log_category val) noexcept
    {
        return static_cast<uint32_t>(val) != 0u;
    }

    class logger_t
    {
    public:
        static void init();
        static void shutdown();

        static void add_sink(std::unique_ptr<log_sink_t> sink);
        static void remove_all_sinks();
        static void flush();

        static void log(log_category category, log_severity severity, const std::string& message,
                        const std::source_location& loc = std::source_location::current());

        static std::string category_to_string(log_category categories);
        static const char* severity_to_string(log_severity level);

        static void set_enabled_categories(const log_category categories) { _enabled_categories = categories; }
        static void set_minimum_severity(const log_severity severity) { _min_severity = severity; }

        // Default enabled categories
        static constexpr log_category default_categories{
            log_category::core | log_category::graphics | log_category::audio | log_category::physics |
            log_category::input | log_category::ui
        };

    private:
        static void internal_log(const log_message& msg);

        static log_category _enabled_categories;
        static log_severity _min_severity;

        static std::vector<std::unique_ptr<log_sink_t>> _sinks;
        static std::mutex _sinks_mutex;




        // static void set_console_color(log_severity level);
        // static void reset_console_color();
        // static void output_with_color(log_severity level, const std::string& text);
        //
        // static log_category _enabled_categories;
        // static log_severity _min_severity;
    };

    // Static member definition
    inline log_category logger_t::_enabled_categories = logger_t::default_categories;
    inline log_severity logger_t::_min_severity = log_severity::trace;
} // namespace carrot::core

#define LOG_CORE_TRACE(...) carrot::core::logger_t::log(carrot::core::log_category::core,           carrot::core::log_severity::trace, __VA_ARGS__)
#define LOG_CORE_DEBUG(...) carrot::core::logger_t::log(carrot::core::log_category::core,           carrot::core::log_severity::debug, __VA_ARGS__)
#define LOG_CORE_INFO(...)  carrot::core::logger_t::log(carrot::core::log_category::core,           carrot::core::log_severity::info, __VA_ARGS__)
#define LOG_CORE_WARN(...)  carrot::core::logger_t::log(carrot::core::log_category::core,           carrot::core::log_severity::warn, __VA_ARGS__)
#define LOG_CORE_ERROR(...) carrot::core::logger_t::log(carrot::core::log_category::core,           carrot::core::log_severity::error, __VA_ARGS__)
#define LOG_CORE_FATAL(...) carrot::core::logger_t::log(carrot::core::log_category::core,           carrot::core::log_severity::fatal, __VA_ARGS__)

#define LOG_GRAPHICS_TRACE(...) carrot::core::logger_t::log(carrot::core::log_category::graphics,   carrot::core::log_severity::trace, __VA_ARGS__)
#define LOG_GRAPHICS_DEBUG(...) carrot::core::logger_t::log(carrot::core::log_category::graphics,   carrot::core::log_severity::debug, __VA_ARGS__)
#define LOG_GRAPHICS_INFO(...)  carrot::core::logger_t::log(carrot::core::log_category::graphics,   carrot::core::log_severity::info, __VA_ARGS__)
#define LOG_GRAPHICS_WARN(...)  carrot::core::logger_t::log(carrot::core::log_category::graphics,   carrot::core::log_severity::warn, __VA_ARGS__)
#define LOG_GRAPHICS_ERROR(...) carrot::core::logger_t::log(carrot::core::log_category::graphics,   carrot::core::log_severity::error, __VA_ARGS__)
#define LOG_GRAPHICS_FATAL(...) carrot::core::logger_t::log(carrot::core::log_category::graphics,   carrot::core::log_severity::fatal, __VA_ARGS__)

#define LOG_AUDIO_TRACE(...) carrot::core::logger_t::log(carrot::core::log_category::audio,         carrot::core::log_severity::trace, __VA_ARGS__)
#define LOG_AUDIO_DEBUG(...) carrot::core::logger_t::log(carrot::core::log_category::audio,         carrot::core::log_severity::debug, __VA_ARGS__)
#define LOG_AUDIO_INFO(...)  carrot::core::logger_t::log(carrot::core::log_category::audio,         carrot::core::log_severity::info, __VA_ARGS__)
#define LOG_AUDIO_WARN(...)  carrot::core::logger_t::log(carrot::core::log_category::audio,         carrot::core::log_severity::warn, __VA_ARGS__)
#define LOG_AUDIO_ERROR(...) carrot::core::logger_t::log(carrot::core::log_category::audio,         carrot::core::log_severity::error, __VA_ARGS__)
#define LOG_AUDIO_FATAL(...) carrot::core::logger_t::log(carrot::core::log_category::audio,         carrot::core::log_severity::fatal, __VA_ARGS__)

#define LOG_PHYSICS_TRACE(...) carrot::core::logger_t::log(carrot::core::log_category::physics,     carrot::core::log_severity::trace, __VA_ARGS__)
#define LOG_PHYSICS_DEBUG(...) carrot::core::logger_t::log(carrot::core::log_category::physics,     carrot::core::log_severity::debug, __VA_ARGS__)
#define LOG_PHYSICS_INFO(...)  carrot::core::logger_t::log(carrot::core::log_category::physics,     carrot::core::log_severity::info, __VA_ARGS__)
#define LOG_PHYSICS_WARN(...)  carrot::core::logger_t::log(carrot::core::log_category::physics,     carrot::core::log_severity::warn, __VA_ARGS__)
#define LOG_PHYSICS_ERROR(...) carrot::core::logger_t::log(carrot::core::log_category::physics,     carrot::core::log_severity::error, __VA_ARGS__)
#define LOG_PHYSICS_FATAL(...) carrot::core::logger_t::log(carrot::core::log_category::physics,     carrot::core::log_severity::fatal, __VA_ARGS__)

#define LOG_INPUT_TRACE(...) carrot::core::logger_t::log(carrot::core::log_category::input,         carrot::core::log_severity::trace, __VA_ARGS__)
#define LOG_INPUT_DEBUG(...) carrot::core::logger_t::log(carrot::core::log_category::input,         carrot::core::log_severity::debug, __VA_ARGS__)
#define LOG_INPUT_INFO(...)  carrot::core::logger_t::log(carrot::core::log_category::input,         carrot::core::log_severity::info, __VA_ARGS__)
#define LOG_INPUT_WARN(...)  carrot::core::logger_t::log(carrot::core::log_category::input,         carrot::core::log_severity::warn, __VA_ARGS__)
#define LOG_INPUT_ERROR(...) carrot::core::logger_t::log(carrot::core::log_category::input,         carrot::core::log_severity::error, __VA_ARGS__)
#define LOG_INPUT_FATAL(...) carrot::core::logger_t::log(carrot::core::log_category::input,         carrot::core::log_severity::fatal, __VA_ARGS__)

#define LOG_NETWORK_TRACE(...) carrot::core::logger_t::log(carrot::core::log_category::network,     carrot::core::log_severity::trace, __VA_ARGS__)
#define LOG_NETWORK_DEBUG(...) carrot::core::logger_t::log(carrot::core::log_category::network,     carrot::core::log_severity::debug, __VA_ARGS__)
#define LOG_NETWORK_INFO(...)  carrot::core::logger_t::log(carrot::core::log_category::network,     carrot::core::log_severity::info, __VA_ARGS__)
#define LOG_NETWORK_WARN(...)  carrot::core::logger_t::log(carrot::core::log_category::network,     carrot::core::log_severity::warn, __VA_ARGS__)
#define LOG_NETWORK_ERROR(...) carrot::core::logger_t::log(carrot::core::log_category::network,     carrot::core::log_severity::error, __VA_ARGS__)
#define LOG_NETWORK_FATAL(...) carrot::core::logger_t::log(carrot::core::log_category::network,     carrot::core::log_severity::fatal, __VA_ARGS__)

#define LOG_UI_TRACE(...) carrot::core::logger_t::log(carrot::core::log_category::ui,               carrot::core::log_severity::trace, __VA_ARGS__)
#define LOG_UI_DEBUG(...) carrot::core::logger_t::log(carrot::core::log_category::ui,               carrot::core::log_severity::debug, __VA_ARGS__)
#define LOG_UI_INFO(...)  carrot::core::logger_t::log(carrot::core::log_category::ui,               carrot::core::log_severity::info, __VA_ARGS__)
#define LOG_UI_WARN(...)  carrot::core::logger_t::log(carrot::core::log_category::ui,               carrot::core::log_severity::warn, __VA_ARGS__)
#define LOG_UI_ERROR(...) carrot::core::logger_t::log(carrot::core::log_category::ui,               carrot::core::log_severity::error, __VA_ARGS__)
#define LOG_UI_FATAL(...) carrot::core::logger_t::log(carrot::core::log_category::ui,               carrot::core::log_severity::fatal, __VA_ARGS__)

#define LOG_ASSET_TRACE(...) carrot::core::logger_t::log(carrot::core::log_category::asset,         carrot::core::log_severity::trace, __VA_ARGS__)
#define LOG_ASSET_DEBUG(...) carrot::core::logger_t::log(carrot::core::log_category::asset,         carrot::core::log_severity::debug, __VA_ARGS__)
#define LOG_ASSET_INFO(...)  carrot::core::logger_t::log(carrot::core::log_category::asset,         carrot::core::log_severity::info, __VA_ARGS__)
#define LOG_ASSET_WARN(...)  carrot::core::logger_t::log(carrot::core::log_category::asset,         carrot::core::log_severity::warn, __VA_ARGS__)
#define LOG_ASSET_ERROR(...) carrot::core::logger_t::log(carrot::core::log_category::asset,         carrot::core::log_severity::error, __VA_ARGS__)
#define LOG_ASSET_FATAL(...) carrot::core::logger_t::log(carrot::core::log_category::asset,         carrot::core::log_severity::fatal, __VA_ARGS__)

#define LOG_SCRIPT_TRACE(...) carrot::core::logger_t::log(carrot::core::log_category::script,       carrot::core::log_severity::trace, __VA_ARGS__)
#define LOG_SCRIPT_DEBUG(...) carrot::core::logger_t::log(carrot::core::log_category::script,       carrot::core::log_severity::debug, __VA_ARGS__)
#define LOG_SCRIPT_INFO(...)  carrot::core::logger_t::log(carrot::core::log_category::script,       carrot::core::log_severity::info, __VA_ARGS__)
#define LOG_SCRIPT_WARN(...)  carrot::core::logger_t::log(carrot::core::log_category::script,       carrot::core::log_severity::warn, __VA_ARGS__)
#define LOG_SCRIPT_ERROR(...) carrot::core::logger_t::log(carrot::core::log_category::script,       carrot::core::log_severity::error, __VA_ARGS__)
#define LOG_SCRIPT_FATAL(...) carrot::core::logger_t::log(carrot::core::log_category::script,       carrot::core::log_severity::fatal, __VA_ARGS__)
