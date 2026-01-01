//
// Created by zshrout on 1/1/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#include "LogSink.h"

namespace carrot::core {
    // ── console_sink_t ──────────────────────────────────────────
    // PUBLIC
    void console_sink_t::write(const log_message& msg)
    {
        const std::string formatted = format_message(msg);
        set_console_color(msg.severity);
        std::println(stdout, "{}", formatted);
        reset_console_color();

        if (msg.severity >= log_severity::error)
        {
            set_console_color(msg.severity);
            std::println(stderr, "{}", formatted);
            reset_console_color();
        }
    }

    // PRIVATE
    std::string console_sink_t::format_message(const log_message& msg)
    {
        std::string cat_str{ logger_t::category_to_string(msg.category) };
        std::string sep{ cat_str.empty() ? "" : " | " };

        return std::format("[{}{}{}] {}:{} {}", cat_str, sep, logger_t::severity_to_string(msg.severity),
                           msg.location.file_name(), msg.location.line(), msg.message);
    }

    void console_sink_t::set_console_color(const log_severity level)
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

    void console_sink_t::reset_console_color()
    {
#ifdef _WIN32
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
#else
        std::print(stdout, "\033[0m");
#endif
    }

    // ── async_sink_t ────────────────────────────────────────────
    // PUBLIC
    async_sink_t::async_sink_t(std::unique_ptr<log_sink_t> wrapped_sink) : _sink{ std::move(wrapped_sink) }
    {
        _thread = std::thread(&async_sink_t::worker_thread, this);
    }

    async_sink_t::~async_sink_t()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _quit = true;
            _cv.notify_one();
        }
        if (_thread.joinable())
            _thread.join();
    }

    void async_sink_t::write(const log_message& msg)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push({ msg, false });
        _cv.notify_one();
    }

    void async_sink_t::flush()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push({ { }, true });
        _cv.notify_one();
    }

    // PRIVATE
    void async_sink_t::worker_thread()
    {
        while (true)
        {
            queue_item item;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _cv.wait(lock, [this] { return !_queue.empty() || _quit; });

                if (_queue.empty())
                {
                    if (_quit) break;
                    continue;
                }

                item = std::move(_queue.front());
                _queue.pop();
            }

            if (item.flush_request)
            {
                _sink->flush();
            }
            else
            {
                _sink->write(item.msg);
            }
        }

        // Drain remaining messages on quit
        std::lock_guard<std::mutex> lock(_mutex);
        while (!_queue.empty())
        {
            auto& item = _queue.front();
            if (!item.flush_request)
                _sink->write(item.msg);
            _queue.pop();
        }
        _sink->flush();
    }
} // namespace carrot::core
