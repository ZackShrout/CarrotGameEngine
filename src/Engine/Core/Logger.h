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

    struct log_message
    {
        log_category category;
        log_severity severity;
        std::string message;
        std::source_location location;
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

        template<typename... Args>
        static void log(const std::source_location& loc, log_category category, log_severity severity,
                        std::format_string<Args...> fmt, Args&&... args);

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
    };

    template<typename... Args>
    void logger_t::log(const std::source_location& loc, const log_category category, const log_severity severity,
                       std::format_string<Args...> fmt, Args&&... args)
    {
        if (severity < _min_severity) return;
        if ((category & _enabled_categories) == static_cast<log_category>(0)) return;

        const std::string message{ std::format(fmt, std::forward<Args>(args)...) };
        const log_message msg{ category, severity, message, loc };

        internal_log(msg);
    }

    // Static member definition
    inline log_category logger_t::_enabled_categories = logger_t::default_categories;
    inline log_severity logger_t::_min_severity = log_severity::trace;
} // namespace carrot::core

#define LOG_IMPL(cat, sev, ...)             \
    carrot::core::logger_t::log(            \
        std::source_location::current(),    \
        carrot::core::log_category::cat,    \
        carrot::core::log_severity::sev,    \
        __VA_ARGS__)

#define LOG_CORE_TRACE(...) LOG_IMPL(core, trace, __VA_ARGS__)
#define LOG_CORE_DEBUG(...) LOG_IMPL(core, debug, __VA_ARGS__)
#define LOG_CORE_INFO(...)  LOG_IMPL(core, info, __VA_ARGS__)
#define LOG_CORE_WARN(...)  LOG_IMPL(core, warn, __VA_ARGS__)
#define LOG_CORE_ERROR(...) LOG_IMPL(core, error, __VA_ARGS__)
#define LOG_CORE_FATAL(...) LOG_IMPL(core, fatal, __VA_ARGS__)

#define LOG_GRAPHICS_TRACE(...) LOG_IMPL(graphics, trace, __VA_ARGS__)
#define LOG_GRAPHICS_DEBUG(...) LOG_IMPL(graphics, debug, __VA_ARGS__)
#define LOG_GRAPHICS_INFO(...)  LOG_IMPL(graphics, info, __VA_ARGS__)
#define LOG_GRAPHICS_WARN(...)  LOG_IMPL(graphics, warn, __VA_ARGS__)
#define LOG_GRAPHICS_ERROR(...) LOG_IMPL(graphics, error, __VA_ARGS__)
#define LOG_GRAPHICS_FATAL(...) LOG_IMPL(graphics, fatal, __VA_ARGS__)

#define LOG_AUDIO_TRACE(...) LOG_IMPL(audio, trace, __VA_ARGS__)
#define LOG_AUDIO_DEBUG(...) LOG_IMPL(audio, debug, __VA_ARGS__)
#define LOG_AUDIO_INFO(...)  LOG_IMPL(audio, info, __VA_ARGS__)
#define LOG_AUDIO_WARN(...)  LOG_IMPL(audio, warn, __VA_ARGS__)
#define LOG_AUDIO_ERROR(...) LOG_IMPL(audio, error, __VA_ARGS__)
#define LOG_AUDIO_FATAL(...) LOG_IMPL(audio, fatal, __VA_ARGS__)

#define LOG_PHYSICS_TRACE(...) LOG_IMPL(physics, trace, __VA_ARGS__)
#define LOG_PHYSICS_DEBUG(...) LOG_IMPL(physics, debug, __VA_ARGS__)
#define LOG_PHYSICS_INFO(...)  LOG_IMPL(physics, info, __VA_ARGS__)
#define LOG_PHYSICS_WARN(...)  LOG_IMPL(physics, warn, __VA_ARGS__)
#define LOG_PHYSICS_ERROR(...) LOG_IMPL(physics, error, __VA_ARGS__)
#define LOG_PHYSICS_FATAL(...) LOG_IMPL(physics, fatal, __VA_ARGS__)

#define LOG_INPUT_TRACE(...) LOG_IMPL(input, trace, __VA_ARGS__)
#define LOG_INPUT_DEBUG(...) LOG_IMPL(input, debug, __VA_ARGS__)
#define LOG_INPUT_INFO(...)  LOG_IMPL(input, info, __VA_ARGS__)
#define LOG_INPUT_WARN(...)  LOG_IMPL(input, warn, __VA_ARGS__)
#define LOG_INPUT_ERROR(...) LOG_IMPL(input, error, __VA_ARGS__)
#define LOG_INPUT_FATAL(...) LOG_IMPL(input, fatal, __VA_ARGS__)

#define LOG_NETWORK_TRACE(...) LOG_IMPL(network, trace, __VA_ARGS__)
#define LOG_NETWORK_DEBUG(...) LOG_IMPL(network, debug, __VA_ARGS__)
#define LOG_NETWORK_INFO(...)  LOG_IMPL(network, info, __VA_ARGS__)
#define LOG_NETWORK_WARN(...)  LOG_IMPL(network, warn, __VA_ARGS__)
#define LOG_NETWORK_ERROR(...) LOG_IMPL(network, error, __VA_ARGS__)
#define LOG_NETWORK_FATAL(...) LOG_IMPL(network, fatal, __VA_ARGS__)

#define LOG_UI_TRACE(...) LOG_IMPL(ui, trace, __VA_ARGS__)
#define LOG_UI_DEBUG(...) LOG_IMPL(ui, debug, __VA_ARGS__)
#define LOG_UI_INFO(...)  LOG_IMPL(ui, info, __VA_ARGS__)
#define LOG_UI_WARN(...)  LOG_IMPL(ui, warn, __VA_ARGS__)
#define LOG_UI_ERROR(...) LOG_IMPL(ui, error, __VA_ARGS__)
#define LOG_UI_FATAL(...) LOG_IMPL(ui, fatal, __VA_ARGS__)

#define LOG_ASSET_TRACE(...) LOG_IMPL(asset, trace, __VA_ARGS__)
#define LOG_ASSET_DEBUG(...) LOG_IMPL(asset, debug, __VA_ARGS__)
#define LOG_ASSET_INFO(...)  LOG_IMPL(asset, info, __VA_ARGS__)
#define LOG_ASSET_WARN(...)  LOG_IMPL(asset, warn, __VA_ARGS__)
#define LOG_ASSET_ERROR(...) LOG_IMPL(asset, error, __VA_ARGS__)
#define LOG_ASSET_FATAL(...) LOG_IMPL(asset, fatal, __VA_ARGS__)

#define LOG_SCRIPT_TRACE(...) LOG_IMPL(script, trace, __VA_ARGS__)
#define LOG_SCRIPT_DEBUG(...) LOG_IMPL(script, debug, __VA_ARGS__)
#define LOG_SCRIPT_INFO(...)  LOG_IMPL(script, info, __VA_ARGS__)
#define LOG_SCRIPT_WARN(...)  LOG_IMPL(script, warn, __VA_ARGS__)
#define LOG_SCRIPT_ERROR(...) LOG_IMPL(script, error, __VA_ARGS__)
#define LOG_SCRIPT_FATAL(...) LOG_IMPL(script, fatal, __VA_ARGS__)
