//
// Created by Zack Shrout on 1/30/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Logger.h"

#include <AppKit/AppKit.h>

namespace carrot::core::platform {

    struct app_delegate_info_t {
        std::string_view _window_title;
        CGRect           _window_rect;
    };

    class cocoa_window_t;
} // namespace carrot::core::platform

@interface app_delegate_t : NSObject <NSApplicationDelegate, NSWindowDelegate>
{
    carrot::core::platform::cocoa_window_t* _owner;
}

- (instancetype)initWithInfo:(const carrot::core::platform::app_delegate_info_t &)info owner:(carrot::core::platform::cocoa_window_t*)owner;

- (void)applicationWillFinishLaunching:(NSNotification *)notification;
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender;
- (void)windowWillClose:(NSNotification *)notification;
- (void)windowDidResize:(NSNotification *)notification;
- (void)windowDidEnterFullScreen:(NSNotification *)notification;
- (void)windowDidExitFullScreen:(NSNotification *)notification;
- (NSWindow *)createAndReturnWindow;

@property (nonatomic, readonly) NSWindow *window;
@property (nonatomic, readonly) carrot::core::platform::app_delegate_info_t info;

@end
