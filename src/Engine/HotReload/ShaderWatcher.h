#pragma once

#include <string>
#include <functional>

namespace carrot::hot_reload {

using shader_reload_callback_t = std::function<void(const std::string& spv_path)>;

class shader_watcher_t
{
public:
    static void init(const shader_reload_callback_t& callback) noexcept;
    static void shutdown() noexcept;
    static void poll() noexcept;        // call every frame from application_t::run()

private:
    static int                      _inotify_fd;
    static int                      _watch_desc;
    static shader_reload_callback_t _callback;
};

} // namespace carrot::hot_reload