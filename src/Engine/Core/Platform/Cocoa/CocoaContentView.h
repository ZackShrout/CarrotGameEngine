//
// Created by Zack Shrout on 3/23/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#import <AppKit/AppKit.h>

namespace carrot::core::platform {
    class cocoa_window_t;
}

@interface cocoa_content_view_t : NSView
- (instancetype)initWithFrame:(NSRect)frame owner:(carrot::core::platform::cocoa_window_t*)owner;
@end
