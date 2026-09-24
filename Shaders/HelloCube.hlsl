cbuffer DrawConstants : register(b0)
{
    float4x4 modelViewProjection;
};

static const float3 positions[8] =
{
    float3(-1, -1, -1), float3(1, -1, -1),
    float3(1, 1, -1), float3(-1, 1, -1),
    float3(-1, -1, 1), float3(1, -1, 1),
    float3(1, 1, 1), float3(-1, 1, 1)
};

static const uint indices[36] =
{
    0, 2, 1, 0, 3, 2,
    4, 5, 6, 4, 6, 7,
    0, 4, 7, 0, 7, 3,
    1, 2, 6, 1, 6, 5,
    3, 7, 6, 3, 6, 2,
    0, 1, 5, 0, 5, 4
};

static const float3 colors[6] =
{
    float3(0.9, 0.2, 0.2), float3(0.2, 0.8, 0.3),
    float3(0.2, 0.4, 0.9), float3(0.9, 0.7, 0.2),
    float3(0.7, 0.3, 0.9), float3(0.2, 0.8, 0.8)
};

struct VertexOut
{
    float4 position : SV_Position;
    float3 color    : COLOR0;
};

VertexOut VSMain(uint id : SV_VertexID)
{
    VertexOut output;
    output.position = mul(float4(positions[indices[id]], 1.0), modelViewProjection);
    output.color = colors[id / 6];
    return output;
}

float4 PSMain(VertexOut input) : SV_Target0
{
    return float4(input.color, 1.0);
}
