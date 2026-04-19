#include "ShaderCommon.h"

struct PointLightData
{
    float4 position_radius;
    float4 color_intensity;
};

struct ForwardPlusTileHeader
{
    uint4 data;
};

struct ForwardPlusFrameConstants
{
    float4 grid_params;
    uint4 tile_counts;
    uint4 point_light_counts;
};
