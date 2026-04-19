#include "ShaderCommon.h"

struct ComputeFillConstants
{
    uint value;
    uint element_count;
    uint dst_byte_offset;
    uint _padding;
};

CARROT_VK_BINDING(0, 0)
RWByteAddressBuffer g_storage0;

CARROT_VK_BINDING(1, 0)
RWByteAddressBuffer g_storage1;

CARROT_VK_BINDING(2, 0)
RWByteAddressBuffer g_storage2;

CARROT_VK_BINDING(3, 0)
RWByteAddressBuffer g_storage3;

CARROT_DECLARE_PUSH_CONSTANT(ComputeFillConstants, g_constants, CARROT_COMPUTE_CONSTANT_REGISTER);

CARROT_ROOT_SIGNATURE(CARROT_RS_COMPUTE)
[numthreads(64, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_constants.element_count)
        return;

    const uint byte_offset = g_constants.dst_byte_offset + dispatch_thread_id.x * 4u;
    g_storage0.Store(byte_offset, g_constants.value);
}
