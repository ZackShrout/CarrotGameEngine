#include "Engine/Core/Application.h"

#include <Engine/Window/Window.h>
#include <Engine/Renderer/VulkanRenderer.h>
#include <Engine/HotReload/ShaderWatcher.h>
#include <Engine/Debug/DebugOverlay.h>

#include <chrono>

using namespace std::chrono;

namespace carrot {

application_t& application_t::get() noexcept
{
    static application_t instance;
    return instance;
}

application_t::application_t() noexcept
{
    init();
}

application_t::~application_t()
{
    shutdown();
}

void application_t::init()
{
    window::create_primary_window(1280, 720, "Carrot Engine – Month 1");
    vulkan_renderer_t::init();
    hot_reload::shader_watcher_t::init([](const std::string& spv_path)
    {
        vulkan_renderer_t::reload_pipeline();
    });
}

void application_t::shutdown()
{
    debug::shutdown();
    hot_reload::shader_watcher_t::shutdown();
    vulkan_renderer_t::shutdown();
    window::destroy_primary_window();
}

void application_t::run()
{
    auto& main_window = window::get_primary_window();

    _last_tick_time = duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()
    ).count();

    while (!_should_quit && !main_window.should_close())
    {
        window::poll_events();
        hot_reload::shader_watcher_t::poll();
        tick();

        vulkan_renderer_t::begin_frame();
        vulkan_renderer_t::render_frame();   // temporary — just our spinning triangle for now

        // Initialize debug overlay AFTER first swapchain image exists
        if (!_debug_overlay_initialized)
        {
            debug::init();
            _debug_overlay_initialized = true;
        }

        vulkan_renderer_t::end_frame();
    }
}

void application_t::tick()
{
    const auto now_ms = duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()
    ).count();

    _delta_time = (now_ms - _last_tick_time) / 1000.f;
    _last_tick_time = now_ms;

    _fps_timer += _delta_time;
    ++_frame_counter;

    if (_fps_timer >= 1.0f)
    {
        _current_fps = _frame_counter;
        _frame_counter = 0;
        _fps_timer -= 1.0f;
    }

    debug::text(20.f, 30.f,  "FPS: %u", _current_fps);
    debug::text(20.f, 65.f, "Frame: %.3f ms", _delta_time * 1000.f);
}

} // namespace carrot