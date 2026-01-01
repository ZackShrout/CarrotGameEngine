//
// Created by zshrout on 12/31/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#include "Logger.h"

namespace carrot::core {
    // PUBLIC
    void logger_t::log(const log_category category, const log_severity severity, const std::string& message, const std::source_location& loc)
    {
        if (severity < _min_severity) return;
        if ((category & _enabled_categories) == 0) return;

        std::string cat_str{ category_to_string(category) };
        std::string sep{ cat_str.empty() ? "" : " | " };
        const std::string prefix = std::format("[{}{}{}] {}:{} ",
                                               cat_str,
                                               sep,
                                               severity_to_string(severity),
                                               loc.file_name(),
                                               loc.line());

        output_with_color(severity, prefix + message);
    }

    // PRIVATE
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

    void logger_t::set_console_color(const log_severity level)
    {
#ifdef _WIN32
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        switch (level)
        {
            case log_severity::trace: SetConsoleTextAttribute(hConsole, 7);
                break; // gray
            case log_severity::debug: SetConsoleTextAttribute(hConsole, 15);
                break; // white
            case log_severity::info: SetConsoleTextAttribute(hConsole, 10);
                break; // light green
            case log_severity::warn: SetConsoleTextAttribute(hConsole, 14);
                break; // yellow
            case log_severity::error: SetConsoleTextAttribute(hConsole, 12);
                break; // light red
            case log_severity::fatal: SetConsoleTextAttribute(hConsole, 4 | 8 | BACKGROUND_RED);
                break; // bright red bg
        }
#else
        switch (level)
        {
            case log_severity::trace: std::print(stdout, "\033[90m");
                break; // gray
            case log_severity::debug: std::print(stdout, "\033[37m");
                break; // white
            case log_severity::info: std::print(stdout, "\033[92m");
                break; // light green
            case log_severity::warn: std::print(stdout, "\033[93m");
                break; // yellow
            case log_severity::error: std::print(stdout, "\033[91m");
                break; // light red
            case log_severity::fatal: std::print(stdout, "\033[97;41m");
                break; // white on red
        }
#endif
    }

    void logger_t::reset_console_color()
    {
#ifdef _WIN32
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
#else
        std::print(stdout, "\033[0m");
#endif
    }

    void logger_t::output_with_color(log_severity level, const std::string& text)
    {
        set_console_color(level);
        std::println(stdout, "{}", text);
        reset_console_color();

        // Also write to stderr for Error/Fatal
        if (level >= log_severity::error)
        {
            set_console_color(level);
            std::print(stderr, "{}", text);
            reset_console_color();
        }
    }
} // namespace carrot::core