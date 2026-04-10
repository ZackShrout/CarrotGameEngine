//
// Created by zshrout on 4/10/26.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "X11Window.h"

#include "Events/Events.h"
#include "Input/PlatformKeyMapping.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <cstdlib>
#include <unistd.h>

namespace carrot::core::platform {
    namespace {
        [[nodiscard]] bool env_flag_enabled(const char* name) noexcept
        {
            const char* value{ std::getenv(name) };
            if (!value || value[0] == '\0')
                return false;

            return std::strcmp(value, "0") != 0
                && std::strcmp(value, "false") != 0
                && std::strcmp(value, "FALSE") != 0
                && std::strcmp(value, "off") != 0
                && std::strcmp(value, "OFF") != 0;
        }

        struct shared_x11_state_t
        {
            struct diagnostics_state_t
            {
                bool enabled{ false };
                bool self_test_enabled{ false };
                bool initialized{ false };
                std::chrono::steady_clock::time_point start_time{ };
                std::size_t next_step{ 0 };
            };

            Display* display{ nullptr };
            Atom wm_protocols{ 0 };
            Atom wm_delete_window{ 0 };
            Atom wm_change_state{ 0 };
            Atom net_active_window{ 0 };
            Atom net_wm_name{ 0 };
            Atom utf8_string{ 0 };
            Atom net_wm_pid{ 0 };
            Atom net_wm_state{ 0 };
            Atom net_wm_state_fullscreen{ 0 };
            Atom net_wm_state_hidden{ 0 };
            Atom net_wm_state_maximized_horz{ 0 };
            Atom net_wm_state_maximized_vert{ 0 };
            Atom net_wm_window_type{ 0 };
            Atom net_wm_window_type_normal{ 0 };
            std::unordered_map<::Window, x11_window_t*> windows;
            uint32_t ref_count{ 0 };
            uint64_t poll_generation{ 0 };
            uint64_t last_pumped_generation{ 0 };
            diagnostics_state_t diagnostics;
        };

        shared_x11_state_t g_shared_x11;

        [[nodiscard]] Atom intern_atom(const char* name) noexcept
        {
            return g_shared_x11.display ? XInternAtom(g_shared_x11.display, name, False) : 0;
        }

        [[nodiscard]] bool acquire_shared_x11_state() noexcept
        {
            if (g_shared_x11.ref_count++ > 0)
                return g_shared_x11.display != nullptr;

            g_shared_x11 = shared_x11_state_t{ };
            g_shared_x11.ref_count = 1;
            g_shared_x11.display = XOpenDisplay(nullptr);
            if (!g_shared_x11.display)
                return false;

            Bool detectable_repeat_supported{ False };
            (void)XkbSetDetectableAutoRepeat(g_shared_x11.display, True, &detectable_repeat_supported);

            g_shared_x11.wm_protocols = intern_atom("WM_PROTOCOLS");
            g_shared_x11.wm_delete_window = intern_atom("WM_DELETE_WINDOW");
            g_shared_x11.wm_change_state = intern_atom("WM_CHANGE_STATE");
            g_shared_x11.net_active_window = intern_atom("_NET_ACTIVE_WINDOW");
            g_shared_x11.net_wm_name = intern_atom("_NET_WM_NAME");
            g_shared_x11.utf8_string = intern_atom("UTF8_STRING");
            g_shared_x11.net_wm_pid = intern_atom("_NET_WM_PID");
            g_shared_x11.net_wm_state = intern_atom("_NET_WM_STATE");
            g_shared_x11.net_wm_state_fullscreen = intern_atom("_NET_WM_STATE_FULLSCREEN");
            g_shared_x11.net_wm_state_hidden = intern_atom("_NET_WM_STATE_HIDDEN");
            g_shared_x11.net_wm_state_maximized_horz = intern_atom("_NET_WM_STATE_MAXIMIZED_HORZ");
            g_shared_x11.net_wm_state_maximized_vert = intern_atom("_NET_WM_STATE_MAXIMIZED_VERT");
            g_shared_x11.net_wm_window_type = intern_atom("_NET_WM_WINDOW_TYPE");
            g_shared_x11.net_wm_window_type_normal = intern_atom("_NET_WM_WINDOW_TYPE_NORMAL");
            g_shared_x11.diagnostics.enabled = env_flag_enabled("CARROT_X11_DIAGNOSTICS");
            g_shared_x11.diagnostics.self_test_enabled = env_flag_enabled("CARROT_X11_SELF_TEST");
            return true;
        }

        void release_shared_x11_state() noexcept
        {
            if (g_shared_x11.ref_count == 0)
                return;

            if (--g_shared_x11.ref_count > 0)
                return;

            if (g_shared_x11.display)
                XCloseDisplay(g_shared_x11.display);

            g_shared_x11 = shared_x11_state_t{ };
        }

        void register_x11_window(x11_window_t* window) noexcept
        {
            if (window && window->get_window() != 0)
                g_shared_x11.windows[window->get_window()] = window;
        }

        void unregister_x11_window(x11_window_t* window) noexcept
        {
            if (!window)
                return;

            g_shared_x11.windows.erase(window->get_window());
        }

        void pump_x11_events() noexcept
        {
            if (!g_shared_x11.display)
                return;

            while (XPending(g_shared_x11.display) > 0)
            {
                XEvent event{ };
                XNextEvent(g_shared_x11.display, &event);

                const auto it{ g_shared_x11.windows.find(event.xany.window) };
                if (it != g_shared_x11.windows.end() && it->second)
                    it->second->handle_event(event);
            }
        }

        x11_window_t* get_primary_diagnostics_window() noexcept
        {
            if (g_shared_x11.windows.empty())
                return nullptr;

            x11_window_t* best_window{ nullptr };
            int best_score{ std::numeric_limits<int>::min() };

            for (const auto& [window_id, window] : g_shared_x11.windows)
            {
                (void)window_id;
                if (!window)
                    continue;

                const std::string_view title{ window->get_title() };
                int score{ 0 };

                if (title.contains("Main Window"))
                    score += 100;
                if (title.contains("Log Console"))
                    score -= 100;
                if (title.contains("Carrot Window"))
                    score += 10;

                if (!best_window || score > best_score)
                {
                    best_window = window;
                    best_score = score;
                }
            }

            return best_window;
        }

        void tick_diagnostics() noexcept
        {
            if (!g_shared_x11.diagnostics.enabled)
                return;

            x11_window_t* window{ get_primary_diagnostics_window() };
            if (!window)
                return;

            if (!g_shared_x11.diagnostics.initialized)
            {
                g_shared_x11.diagnostics.initialized = true;
                g_shared_x11.diagnostics.start_time = std::chrono::steady_clock::now();
                LOG_CORE_INFO("X11 diagnostics enabled");

                if (g_shared_x11.diagnostics.self_test_enabled)
                    LOG_CORE_INFO("X11 self-test enabled: focus -> maximize -> restore -> fullscreen -> restore");
            }

            if (!g_shared_x11.diagnostics.self_test_enabled)
                return;

            using namespace std::chrono_literals;

            struct self_test_step_t
            {
                std::chrono::milliseconds delay;
                const char* description;
                void (*action)(x11_window_t&) noexcept;
            };

            static constexpr std::array<self_test_step_t, 5> steps{ {
                { 1000ms, "request focus", [](x11_window_t& w) noexcept { w.request_focus(); } },
                { 2000ms, "maximize", [](x11_window_t& w) noexcept { w.maximize(); } },
                { 3500ms, "restore", [](x11_window_t& w) noexcept { w.restore(); } },
                { 5000ms, "enter fullscreen", [](x11_window_t& w) noexcept { w.set_fullscreen(true); } },
                { 7000ms, "exit fullscreen", [](x11_window_t& w) noexcept { w.set_fullscreen(false); } },
            } };

            const auto elapsed{
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - g_shared_x11.diagnostics.start_time
                )
            };

            while (g_shared_x11.diagnostics.next_step < steps.size()
                   && elapsed >= steps[g_shared_x11.diagnostics.next_step].delay)
            {
                const auto& step{ steps[g_shared_x11.diagnostics.next_step] };
                LOG_CORE_INFO("X11 self-test step {}: {}", g_shared_x11.diagnostics.next_step + 1, step.description);
                step.action(*window);
                ++g_shared_x11.diagnostics.next_step;
            }

            if (g_shared_x11.diagnostics.next_step == steps.size())
            {
                LOG_CORE_INFO("X11 self-test complete");
                g_shared_x11.diagnostics.self_test_enabled = false;
            }
        }

        [[nodiscard]] KeySym lookup_keysym(::Display* display, const XKeyEvent& event) noexcept
        {
            if (!display)
                return NoSymbol;

            const int column{ (event.state & ShiftMask) != 0 ? 1 : 0 };
            return XkbKeycodeToKeysym(display, static_cast<KeyCode>(event.keycode), 0, column);
        }
    } // anonymous namespace

    x11_window_t::x11_window_t(const uint32_t width, const uint32_t height, const std::string_view title) noexcept
    {
        _width = width;
        _height = height;
        _title = std::string{ title };

        if (!acquire_shared_x11_state())
        {
            LOG_CORE_FATAL("Failed to open X11 display");
            _should_close = true;
            return;
        }

        _display = g_shared_x11.display;
        _screen = DefaultScreen(_display);

        _window = XCreateSimpleWindow(
            _display,
            RootWindow(_display, _screen),
            0,
            0,
            width,
            height,
            1,
            BlackPixel(_display, _screen),
            WhitePixel(_display, _screen)
        );

        if (_window == 0)
        {
            LOG_CORE_FATAL("Failed to create X11 window");
            _should_close = true;
            release_shared_x11_state();
            return;
        }

        _wm_delete_window = g_shared_x11.wm_delete_window;

        XSelectInput(
            _display,
            _window,
            ExposureMask |
            StructureNotifyMask |
            FocusChangeMask |
            PropertyChangeMask |
            KeyPressMask |
            KeyReleaseMask |
            ButtonPressMask |
            ButtonReleaseMask |
            PointerMotionMask
        );

        if (g_shared_x11.wm_delete_window != 0)
        {
            Atom protocols[]{ g_shared_x11.wm_delete_window };
            XSetWMProtocols(_display, _window, protocols, 1);
        }

        XClassHint class_hint{ };
        class_hint.res_name = const_cast<char*>("carrot-engine");
        class_hint.res_class = const_cast<char*>("CarrotEngine");
        XSetClassHint(_display, _window, &class_hint);

        if (g_shared_x11.net_wm_window_type != 0 && g_shared_x11.net_wm_window_type_normal != 0)
        {
            const Atom window_type{ g_shared_x11.net_wm_window_type_normal };
            XChangeProperty(_display,
                            _window,
                            g_shared_x11.net_wm_window_type,
                            XA_ATOM,
                            32,
                            PropModeReplace,
                            reinterpret_cast<const unsigned char*>(&window_type),
                            1);
        }

        if (g_shared_x11.net_wm_pid != 0)
        {
            const pid_t pid{ getpid() };
            XChangeProperty(_display,
                            _window,
                            g_shared_x11.net_wm_pid,
                            XA_CARDINAL,
                            32,
                            PropModeReplace,
                            reinterpret_cast<const unsigned char*>(&pid),
                            1);
        }

        set_title(title);

        XMapWindow(_display, _window);
        XFlush(_display);

        register_x11_window(this);
    }

    x11_window_t::~x11_window_t() noexcept
    {
        unregister_x11_window(this);

        if (_display && _window != 0)
            XDestroyWindow(_display, _window);

        _window = 0;
        _display = nullptr;
        release_shared_x11_state();
    }

    void x11_window_t::poll_events() noexcept
    {
        if (g_shared_x11.last_pumped_generation != g_shared_x11.poll_generation)
        {
            pump_x11_events();
            g_shared_x11.last_pumped_generation = g_shared_x11.poll_generation;
        }
    }

    void x11_window_t::set_should_close(const bool should_close) noexcept
    {
        if (_should_close == should_close)
            return;

        _should_close = should_close;
        if (should_close)
            on_window_closed(events::window_closed_t{ });
    }

    void x11_window_t::set_title(const std::string_view title) noexcept
    {
        _title = std::string{ title };
        if (_display && _window != 0)
        {
            XStoreName(_display, _window, _title.c_str());

            if (g_shared_x11.net_wm_name != 0 && g_shared_x11.utf8_string != 0)
            {
                XChangeProperty(_display,
                                _window,
                                g_shared_x11.net_wm_name,
                                g_shared_x11.utf8_string,
                                8,
                                PropModeReplace,
                                reinterpret_cast<const unsigned char*>(_title.c_str()),
                                static_cast<int>(_title.size()));
            }
        }
    }

    void x11_window_t::minimize() noexcept
    {
        if (_display && _window != 0)
            XIconifyWindow(_display, _window, _screen);
    }

    void x11_window_t::maximize() noexcept
    {
        apply_wm_state_atom(g_shared_x11.net_wm_state_maximized_horz, true);
        apply_wm_state_atom(g_shared_x11.net_wm_state_maximized_vert, true);
        _is_maximized = true;
    }

    void x11_window_t::restore() noexcept
    {
        set_fullscreen(false);
        apply_wm_state_atom(g_shared_x11.net_wm_state_maximized_horz, false);
        apply_wm_state_atom(g_shared_x11.net_wm_state_maximized_vert, false);
        _is_maximized = false;
        if (_display && _window != 0)
            XMapRaised(_display, _window);
    }

    void x11_window_t::request_focus() noexcept
    {
        if (!_display || _window == 0)
            return;

        XMapRaised(_display, _window);

        XEvent event{ };
        event.xclient.type = ClientMessage;
        event.xclient.window = _window;
        event.xclient.message_type = g_shared_x11.net_active_window;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 1;
        event.xclient.data.l[1] = CurrentTime;

        XSendEvent(
            _display,
            RootWindow(_display, _screen),
            False,
            SubstructureRedirectMask | SubstructureNotifyMask,
            &event
        );

        XSetInputFocus(_display, _window, RevertToParent, CurrentTime);
        XFlush(_display);
    }

    native_window_handle_t x11_window_t::get_native_handle() const noexcept
    {
        native_window_handle_t handle{ };
        handle.x11_t.display = _display;
        handle.x11_t.window = _window;
        return handle;
    }

    void x11_window_t::set_fullscreen(const bool fullscreen) noexcept
    {
        if (_is_fullscreen == fullscreen)
            return;

        _is_fullscreen = fullscreen;
        apply_wm_state_atom(g_shared_x11.net_wm_state_fullscreen, fullscreen);
    }

    void x11_window_t::begin_poll_cycle() noexcept
    {
        ++g_shared_x11.poll_generation;
        pump_x11_events();
        tick_diagnostics();
        g_shared_x11.last_pumped_generation = g_shared_x11.poll_generation;
    }

    void x11_window_t::handle_event(const XEvent& event) noexcept
    {
        switch (event.type)
        {
            case ConfigureNotify:
                update_size(static_cast<uint32_t>(event.xconfigure.width),
                            static_cast<uint32_t>(event.xconfigure.height));
                break;
            case FocusIn:
                _is_focused = true;
                if (g_shared_x11.diagnostics.enabled)
                    LOG_CORE_INFO("X11 window '{}' focused", _title);
                on_window_focus_changed(events::window_focused_t{ ._focused = true });
                break;
            case FocusOut:
                _is_focused = false;
                _pressed_keys.clear();
                if (g_shared_x11.diagnostics.enabled)
                    LOG_CORE_INFO("X11 window '{}' focus lost", _title);
                on_window_focus_changed(events::window_focused_t{ ._focused = false });
                break;
            case MapNotify:
                _is_minimized = false;
                if (g_shared_x11.diagnostics.enabled)
                    LOG_CORE_INFO("X11 window '{}' mapped", _title);
                break;
            case UnmapNotify:
                _is_minimized = true;
                if (g_shared_x11.diagnostics.enabled)
                    LOG_CORE_INFO("X11 window '{}' unmapped", _title);
                break;
            case PropertyNotify:
                if (event.xproperty.atom == g_shared_x11.net_wm_state)
                    sync_wm_state();
                break;
            case KeyPress:
                handle_key_press(event.xkey);
                break;
            case KeyRelease:
                handle_key_release(event.xkey);
                break;
            case ButtonPress:
                handle_button_press(event.xbutton);
                break;
            case ButtonRelease:
                handle_button_release(event.xbutton);
                break;
            case MotionNotify:
                handle_motion(event.xmotion);
                break;
            case ClientMessage:
                if (event.xclient.message_type == g_shared_x11.wm_protocols
                    && static_cast<Atom>(event.xclient.data.l[0]) == _wm_delete_window)
                {
                    set_should_close(true);
                }
                break;
            default:
                break;
        }
    }

    void x11_window_t::apply_wm_state_atom(const Atom state_atom, const bool enabled) noexcept
    {
        if (!_display || _window == 0 || g_shared_x11.net_wm_state == 0 || state_atom == 0)
            return;

        XEvent event{ };
        event.xclient.type = ClientMessage;
        event.xclient.serial = 0;
        event.xclient.send_event = True;
        event.xclient.window = _window;
        event.xclient.message_type = g_shared_x11.net_wm_state;
        event.xclient.format = 32;
        event.xclient.data.l[0] = enabled ? 1 : 0;
        event.xclient.data.l[1] = static_cast<long>(state_atom);
        event.xclient.data.l[2] = 0;
        event.xclient.data.l[3] = 1;

        XSendEvent(
            _display,
            RootWindow(_display, _screen),
            False,
            SubstructureRedirectMask | SubstructureNotifyMask,
            &event
        );
        XFlush(_display);
    }

    void x11_window_t::sync_wm_state() noexcept
    {
        if (!_display || _window == 0 || g_shared_x11.net_wm_state == 0)
            return;

        Atom actual_type{ None };
        int actual_format{ 0 };
        unsigned long item_count{ 0 };
        unsigned long bytes_after{ 0 };
        unsigned char* property_data{ nullptr };

        const int status{
            XGetWindowProperty(_display,
                               _window,
                               g_shared_x11.net_wm_state,
                               0,
                               16,
                               False,
                               XA_ATOM,
                               &actual_type,
                               &actual_format,
                               &item_count,
                               &bytes_after,
                               &property_data)
        };

        if (status != Success || actual_type != XA_ATOM || actual_format != 32)
        {
            if (property_data)
                XFree(property_data);
            return;
        }

        bool fullscreen{ false };
        bool hidden{ false };
        bool maximized_horz{ false };
        bool maximized_vert{ false };

        const auto* atoms{ reinterpret_cast<const Atom*>(property_data) };
        for (unsigned long i{ 0 }; i < item_count; ++i)
        {
            if (atoms[i] == g_shared_x11.net_wm_state_fullscreen)
                fullscreen = true;
            else if (atoms[i] == g_shared_x11.net_wm_state_hidden)
                hidden = true;
            else if (atoms[i] == g_shared_x11.net_wm_state_maximized_horz)
                maximized_horz = true;
            else if (atoms[i] == g_shared_x11.net_wm_state_maximized_vert)
                maximized_vert = true;
        }

        XFree(property_data);

        _is_fullscreen = fullscreen;
        _is_minimized = hidden;
        _is_maximized = maximized_horz && maximized_vert;

        if (g_shared_x11.diagnostics.enabled)
        {
            LOG_CORE_INFO("X11 window '{}' WM state synced: fullscreen={}, minimized={}, maximized={}",
                          _title,
                          _is_fullscreen,
                          _is_minimized,
                          _is_maximized);
        }
    }

    void x11_window_t::update_modifiers(const unsigned int state) noexcept
    {
        uint8_t mods{ 0 };
        if ((state & ShiftMask) != 0) mods |= static_cast<uint8_t>(input::modifier::shift);
        if ((state & ControlMask) != 0) mods |= static_cast<uint8_t>(input::modifier::control);
        if ((state & Mod1Mask) != 0) mods |= static_cast<uint8_t>(input::modifier::alt);
        if ((state & Mod4Mask) != 0) mods |= static_cast<uint8_t>(input::modifier::super);
        _mods = mods;
    }

    void x11_window_t::update_size(const uint32_t width, const uint32_t height) noexcept
    {
        _is_resizing = true;

        if (width != _width || height != _height)
        {
            _width = width;
            _height = height;
            on_window_resized(events::window_resized_t{ ._width = _width, ._height = _height });
        }

        _is_resizing = false;
    }

    void x11_window_t::handle_key_press(const XKeyEvent& event) noexcept
    {
        update_modifiers(event.state);
        const KeySym keysym{ lookup_keysym(_display, event) };
        const input::key_code key{ input::to_carrot_key(static_cast<uint32_t>(keysym)) };
        if (key == input::key_code::unknown)
            return;

        const bool already_pressed{ _pressed_keys.contains(key) };
        _pressed_keys.insert(key);
        on_key(events::key_event_t{
            ._key = key,
            ._action = already_pressed ? events::key_action::repeat : events::key_action::press,
            ._repeat = already_pressed,
            ._mods = _mods,
        });
    }

    void x11_window_t::handle_key_release(const XKeyEvent& event) noexcept
    {
        update_modifiers(event.state);
        const KeySym keysym{ lookup_keysym(_display, event) };
        const input::key_code key{ input::to_carrot_key(static_cast<uint32_t>(keysym)) };
        if (key == input::key_code::unknown)
            return;

        _pressed_keys.erase(key);
        on_key(events::key_event_t{
            ._key = key,
            ._action = events::key_action::release,
            ._repeat = false,
            ._mods = _mods,
        });
    }

    void x11_window_t::handle_button_press(const XButtonEvent& event) noexcept
    {
        update_modifiers(event.state);

        if (event.button == Button4 || event.button == Button5)
        {
            const chlm::float2 delta{
                0.f,
                event.button == Button4 ? 1.f : -1.f
            };
            on_mouse_scrolled(events::mouse_scrolled_event_t{ ._delta = delta });
            return;
        }

        on_mouse_button(events::mouse_button_event_t{
            ._button = input::to_carrot_mouse_button(event.button),
            ._action = events::key_action::press,
            ._pos = { static_cast<float>(event.x), static_cast<float>(event.y) },
        });
    }

    void x11_window_t::handle_button_release(const XButtonEvent& event) noexcept
    {
        update_modifiers(event.state);

        if (event.button == Button4 || event.button == Button5)
            return;

        on_mouse_button(events::mouse_button_event_t{
            ._button = input::to_carrot_mouse_button(event.button),
            ._action = events::key_action::release,
            ._pos = { static_cast<float>(event.x), static_cast<float>(event.y) },
        });
    }

    void x11_window_t::handle_motion(const XMotionEvent& event) noexcept
    {
        update_modifiers(event.state);
        const chlm::float2 position{
            static_cast<float>(event.x),
            static_cast<float>(event.y)
        };
        const chlm::float2 delta{ position - _last_mouse_position };
        _last_mouse_position = position;

        on_mouse_moved(events::mouse_moved_event_t{
            ._pos = position,
            ._delta = delta,
        });
    }
} // namespace carrot::core::platform
