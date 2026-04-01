//
// Created by Zack Shrout on 3/25/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#ifndef CARROT_SHADER_COMMON_H
#define CARROT_SHADER_COMMON_H

#if defined(CARROT_USE_ROOT_SIGNATURES)
    #define CARROT_ROOT_SIGNATURE(x) [RootSignature(x)]
#else
    #define CARROT_ROOT_SIGNATURE(x)
#endif

#if defined(VULKAN)
    #define CARROT_VK_BINDING(binding_index, set_index) [[vk::binding(binding_index, set_index)]]
#else
    #define CARROT_VK_BINDING(binding_index, set_index)
#endif

#define CARROT_RS_TEXTURED_QUAD \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
"DescriptorTable(CBV(b0, numDescriptors=1))," \
"DescriptorTable(SRV(t0, numDescriptors=1))," \
"DescriptorTable(Sampler(s0, numDescriptors=1))"

// Backend-specific clip-space normalization.
//
// Carrot's higher-level 2D convention is world-space with a top-left origin
// and +Y pointing down. Once positions started flowing through a shared
// orthographic camera projection instead of being authored directly in clip
// space, the required Y fixup became "which backend disagrees with the clip
// convention produced by that projection?" rather than "which backend was
// upside down in the old direct clip-space test path?"
//
// In the current renderer setup, Vulkan is the backend that needs the sign
// correction while Metal and DirectX 12 naturally match the shared 2D camera
// projection path.
#ifndef CARROT_CLIP_SPACE_Y_SIGN
    #define CARROT_CLIP_SPACE_Y_SIGN 1.0
#endif

#define CARROT_APPLY_CLIP_SPACE_Y(pos) float2((pos).x, (pos).y * CARROT_CLIP_SPACE_Y_SIGN)

#endif
