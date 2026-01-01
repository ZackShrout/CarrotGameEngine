//
// Created by zshrout on 12/31/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#include "Logger.h"

#include "LogSink.h"

namespace carrot::core {
    std::vector<std::unique_ptr<log_sink_t>> logger_t::_sinks;
    std::mutex logger_t::_sinks_mutex;

    // PUBLIC
    void logger_t::init()
    {
        // Prevent double-init (idempotent)
        static bool initialized = false;
        if (initialized) return;
        initialized = true;

        // Create default console sink
        auto console = std::make_unique<console_sink_t>();

        // Wrap it in async sink for non-blocking logging
        auto async_console = std::make_unique<async_sink_t>(std::move(console));

        // Add it
        add_sink(std::move(async_console));

        // Note: We can't use macros yet because sinks just got added, but internal_log bypasses filters,
        //       so we do a direct internal log here.
        const log_message startup_msg{
            log_category::core,
            log_severity::info,
            "Logger initialized with async console sink",
            std::source_location::current()
        };
        internal_log(startup_msg);
    }

    void logger_t::shutdown()
    {
        flush();
        remove_all_sinks();
    }

    void logger_t::add_sink(std::unique_ptr<log_sink_t> sink)
    {
        std::lock_guard<std::mutex> lock(_sinks_mutex);
        _sinks.push_back(std::move(sink));
    }

    void logger_t::remove_all_sinks()
    {
        std::lock_guard<std::mutex> lock(_sinks_mutex);
        _sinks.clear();  // This destroys all sink objects
        // → async_sink destructors join their threads and flush remaining messages
    }

    void logger_t::flush()
    {
        std::lock_guard<std::mutex> lock(_sinks_mutex);
        for (const auto& sink : _sinks)
            sink->flush();
    }

    void logger_t::log(const log_category category, const log_severity severity, const std::string& message,
                       const std::source_location& loc)
    {
        // if (severity < _min_severity) return;
        // if ((category & _enabled_categories) == 0) return;
        //
        // std::string cat_str{ category_to_string(category) };
        // std::string sep{ cat_str.empty() ? "" : " | " };
        // const std::string prefix = std::format("[{}{}{}] {}:{} ",
        //                                        cat_str,
        //                                        sep,
        //                                        severity_to_string(severity),
        //                                        loc.file_name(),
        //                                        loc.line());
        //
        // output_with_color(severity, prefix + message);

        if (severity < _min_severity) return;
        if ((category & _enabled_categories) == static_cast<log_category>(0)) return;

        const log_message msg{ category, severity, message, loc };

        internal_log(msg);
    }

    std::string logger_t::category_to_string(log_category categories)
    {
        struct category_info
        {
            log_category mask;
            const char* name;
        };

        static constexpr category_info infos[] = {
            { log_category::core, "CORE" },
            { log_category::graphics, "GRAPHICS" },
            { log_category::audio, "AUDIO" },
            { log_category::physics, "PHYSICS" },
            { log_category::input, "INPUT" },
            { log_category::network, "NETWORK" },
            { log_category::ui, "UI" },
            { log_category::asset, "ASSET" },
            { log_category::script, "SCRIPT" }
        };

        if (categories == static_cast<log_category>(0)) return "NONE";

        std::string result;
        bool first{ true };
        uint32_t remaining{ static_cast<uint32_t>(categories) };

        for (const auto& [mask, name]: infos)
        {
            if (remaining & static_cast<uint32_t>(mask))
            {
                if (!first) result += "|";
                result += name;
                first = false;
                remaining &= ~static_cast<uint32_t>(mask); // clear bit
            }
        }

        if (remaining != 0)
        {
            if (!first) result += "|";
            result += "???";
        }

        return result.empty() ? "???" : result;
    }

    const char* logger_t::severity_to_string(const log_severity level)
    {
        switch (level)
        {
            case log_severity::trace: return "TRACE";
            case log_severity::debug: return "DEBUG";
            case log_severity::info: return "INFO";
            case log_severity::warn: return "WARN";
            case log_severity::error: return "ERROR";
            case log_severity::fatal: return "FATAL";
            default: return "???";
        }
    }

    // PRIVATE
    void logger_t::internal_log(const log_message& msg)
    {
        std::lock_guard<std::mutex> lock(_sinks_mutex);
        for (const auto& sink: _sinks)
            sink->write(msg);
    }

    // void logger_t::set_console_color(const log_severity level)
    // {
    //
    // }
    //
    // void logger_t::reset_console_color()
    // {
    //
    // }
    //
    // void logger_t::output_with_color(log_severity level, const std::string& text)
    // {
    //     set_console_color(level);
    //     std::println(stdout, "{}", text);
    //     reset_console_color();
    //
    //     // Also write to stderr for Error/Fatal
    //     if (level >= log_severity::error)
    //     {
    //         set_console_color(level);
    //         std::print(stderr, "{}", text);
    //         reset_console_color();
    //     }
    // }
} // namespace carrot::core
