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
} // namespace carrot::core::platform

@interface app_delegate_t : NSObject <NSApplicationDelegate, NSWindowDelegate>

- (instancetype)initWithInfo:(const carrot::core::platform::app_delegate_info_t &)info;

- (void)applicationWillFinishLaunching:(NSNotification *)notification;
- (void)applicationDidFinishLaunching:(NSNotification *)notification;
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender;
- (void)windowWillClose:(NSNotification *)notification;
- (NSWindow *)createAndReturnWindow;

@property (nonatomic, readonly) NSWindow *window;
@property (nonatomic, readonly) carrot::core::platform::app_delegate_info_t info;

@end
