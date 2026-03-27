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

#define CARROT_RS_TEXTURED_QUAD \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
"DescriptorTable(SRV(t0, numDescriptors=1))," \
"DescriptorTable(Sampler(s0, numDescriptors=1))"

#endif