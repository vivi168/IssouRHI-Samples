#include "HelloCommon.hlsli"

cbuffer DrawConstants : register(b0)
{
    float2 scale;
};

static const float4 positions[3] =
{
    float4( 0.0,  0.5, 0.0, 1.0),
    float4( 0.5, -0.5, 0.0, 1.0),
    float4(-0.5, -0.5, 0.0, 1.0),
};

static const float4 colors[3] =
{
    float4(1.0, 0.0, 0.0, 1.0),
    float4(0.0, 1.0, 0.0, 1.0),
    float4(0.0, 0.0, 1.0, 1.0),
};

VertexOut main(uint id : SV_VertexID)
{
    VertexOut output;

    output.position = positions[id];
    output.position.xy *= scale;
    output.color = colors[id];

    return output;
}
