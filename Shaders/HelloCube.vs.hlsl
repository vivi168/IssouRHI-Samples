#include "HelloCommon.hlsli"

cbuffer DrawConstants : register(b0)
{
  float4x4 modelViewProjection;
};

static const float4 positions[8] =
{
  float4(-1, -1, -1, 1), float4( 1, -1, -1, 1),
  float4( 1,  1, -1, 1), float4(-1,  1, -1, 1),
  float4(-1, -1,  1, 1), float4( 1, -1,  1, 1),
  float4( 1,  1,  1, 1), float4(-1,  1,  1, 1),
};

static const uint indices[36] =
{
  0, 2, 1,  0, 3, 2,
  4, 5, 6,  4, 6, 7,
  0, 4, 7,  0, 7, 3,
  1, 2, 6,  1, 6, 5,
  3, 7, 6,  3, 6, 2,
  0, 1, 5,  0, 5, 4,
};

static const float4 colors[6] =
{
  float4(0.9, 0.2, 0.2, 1.0), float4(0.2, 0.8, 0.3, 1.0),
  float4(0.2, 0.4, 0.9, 1.0), float4(0.9, 0.7, 0.2, 1.0),
  float4(0.7, 0.3, 0.9, 1.0), float4(0.2, 0.8, 0.8, 1.0),
};

VertexOut main(uint id : SV_VertexID)
{
  VertexOut output;
  output.position = mul(positions[indices[id]], modelViewProjection);
  output.color = colors[id / 6];

  return output;
}
