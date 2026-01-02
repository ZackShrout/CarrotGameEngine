//
// Created by zshrout on 1/1/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#pragma once

#include "Logger.h"

#include <string>
#include <memory>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <atomic>

namespace carrot::core {
    class log_sink_t
    {
    public:
        virtual ~log_sink_t() = default;
        virtual void write(const log_message& msg) = 0;
        virtual void flush() {}
    };

    class console_sink_t : public log_sink_t
    {
    public:
        void write(const log_message& msg) override;

    private:
        static std::string format_message(const log_message& msg);
        static void set_console_color(log_severity level);
        static void reset_console_color();
    };

    // Async sink that wraps any other sink
    class async_sink_t : public log_sink_t
    {
    public:
        explicit async_sink_t(std::unique_ptr<log_sink_t> wrapped_sink);
        ~async_sink_t() override;

        void write(const log_message& msg) override;
        void flush() override;

    private:
        void worker_thread();

        std::unique_ptr<log_sink_t> _sink;

        struct queue_item {
            log_message msg;
            bool flush_request{false};
        };

        std::queue<queue_item> _queue;
        std::mutex _mutex;
        std::condition_variable _cv;
        std::thread _thread;
        std::atomic<bool> _quit{false};
    };

} // namespace carrot::core