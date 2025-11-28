#include "Engine/HotReload/ShaderWatcher.h"
#include "Engine/Renderer/VulkanRenderer.h"

#include <sys/inotify.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace carrot::hot_reload {

int            shader_watcher_t::_inotify_fd{ -1 };
int            shader_watcher_t::_watch_desc{ -1 };
shader_reload_callback_t shader_watcher_t::_callback;

void shader_watcher_t::init(const shader_reload_callback_t& callback) noexcept
{
    _callback = callback;

    _inotify_fd = inotify_init1(IN_NONBLOCK);
    if (_inotify_fd == -1) return;

    _watch_desc = inotify_add_watch(_inotify_fd, "shaders", IN_CLOSE_WRITE | IN_MOVED_TO);
}

void shader_watcher_t::shutdown() noexcept
{
    if (_watch_desc != -1) inotify_rm_watch(_inotify_fd, _watch_desc);
    if (_inotify_fd != -1) close(_inotify_fd);
    _inotify_fd = -1;
    _watch_desc = -1;
}

void shader_watcher_t::poll() noexcept
{
    if (_inotify_fd == -1) return;

    char buffer[4096];
    ssize_t len = read(_inotify_fd, buffer, sizeof(buffer));
    if (len <= 0) return;

    const inotify_event* event;
    for (char* ptr = buffer; ptr < buffer + len; ptr += sizeof(inotify_event) + event->len) {
        event = (const inotify_event*)ptr;

        if (!(event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO))) continue;
        if (!(event->mask & IN_ISDIR) && (std::string(event->name).ends_with(".vert") ||
                                          std::string(event->name).ends_with(".frag"))) {

            std::string glsl_path = "shaders/" + std::string(event->name);
            std::string spv_path  = "bin/debug/shaders/" + std::string(event->name) + ".spv";

            // Recompile immediately
            std::string cmd = "glslangValidator -V \"" + glsl_path + "\" -o \"" + spv_path + "\"";
            int result = system(cmd.c_str());

            if (result == 0) {
                printf("[HotReload] Recompiled %s\n", event->name);
                // Give filesystem a moment
                usleep(50000);
                if (_callback) _callback(spv_path);
            } else {
                fprintf(stderr, "[HotReload] Compilation failed for %s\n", event->name);
            }
        }
    }
}

} // namespace carrot::hot_reload