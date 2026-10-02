#ifndef TERRAIN_COMMON_HLSLI
#define TERRAIN_COMMON_HLSLI

#include "../Terrain/Shared.h"

ConstantBuffer<TerrainDrawArgs> args : register(b0);

struct VertexOut {
  float4 position : SV_Position;
  float3 color : COLOR0;
  float2 terrainXZ : TEXCOORD0;
};

float2 roundToIncrement(float2 value, float increment)
{
  return round(value * (1.0 / increment)) * increment;
}

float2 TerrainGridToWorldXZ(float2 gridPosition, float gridLevel, TerrainMeasurements terrain)
{
  float mipMetersPerHeightfieldTexel = terrain.metersPerTexel * exp2(gridLevel);
  float2 objectToWorld = roundToIncrement(-args.terrainOrigin.xz, mipMetersPerHeightfieldTexel);
  return gridPosition * terrain.metersPerTexel + objectToWorld;
}

// Adapted from Morgan McGuire, https://casual-effects.blogspot.com/2014/04/fast-terrain-rendering-with-continuous.html
VertexOut EvaluateTerrainVertex(float3 vertex) // Y is LOD level
{
  SamplerState linearClampSampler = SamplerDescriptorHeap[args.samplerId];
  StructuredBuffer<TerrainMeasurements> information = ResourceDescriptorHeap[args.terrainInformationId];
  TerrainMeasurements terrain = information[0];
  Texture2D<float4> heightfield = ResourceDescriptorHeap[args.heightfieldId];

  float3 wsCamera = -args.terrainOrigin.xyz;

  float2 worldXZ = TerrainGridToWorldXZ(vertex.xz, vertex.y, terrain);
  float3 wsPosition = float3(worldXZ.x, 0.0, worldXZ.y);

  // Where is the grid point in the grid's object space?
  // This will determine the level used for elevation fetch
  float3 osPosition = wsPosition - wsCamera;

  // Reach size = 1 at the border of the level 0 grid, which has radius baseGridSizeTexels/2,
  // but never go below 1.0 / 4.0, the "-1" level.
  float2 scaledDistance = abs(osPosition.xz * terrain.heightfieldTexelsPerMeter * TerrainInvBaseGridSizeTexels);
  float size = max(0.5, max(scaledDistance.x, scaledDistance.y));

  // The heightfield texture level must be negatively biased to hit max resolution
  // before the highest resolution grid, otherwise texture swim will result on
  // nearby surfaces. Increase the magnitude of the negative bias term towards -inf if transitions
  // are too obvious. Decrease it towards zero if tessellation is being wasted on surfaces
  // that are too blocky.
  float gridLOD = max(log2(size) - 0.75, 0.0);

  // We want to sample from index (wsPosition.xz * terrain.heightfieldTexelsPerMeter + 0.5), but
  // at the appropriate texture resolution and using texture coordinates.
  float lowMIP = floor(gridLOD);
  float highMIP = min(lowMIP + 1.0, float(TerrainHeightfieldMipCount - 1));

  float fractionalLevel = gridLOD - lowMIP;

  // How many high-level texels to offset to achieve a half-texel offset at this MIP level
  float highMIPHalfTexelOffset = exp2(lowMIP);
  float lowMIPHalfTexelOffset = highMIPHalfTexelOffset * 0.5;

  float2 lowMIPTexCoord  = (wsPosition.xz * terrain.heightfieldTexelsPerMeter + lowMIPHalfTexelOffset) * terrain.heightfield_invSize;
  float2 highMIPTexCoord = (wsPosition.xz * terrain.heightfieldTexelsPerMeter + highMIPHalfTexelOffset) * terrain.heightfield_invSize;

  // Manual trilinear interpolation
  float lowMIPValue;

  if (lowMIP > 0.0) {
    lowMIPValue = heightfield.SampleLevel(linearClampSampler, lowMIPTexCoord, lowMIP).w;
  } else {
    // At the lowest LOD, smooth out sharp corners
    const float smoothness = 0.35;
    lowMIPValue =
        (heightfield.SampleLevel(linearClampSampler, float2( terrain.heightfield_invSize.x,  terrain.heightfield_invSize.y) * smoothness + lowMIPTexCoord, lowMIP).w +
         heightfield.SampleLevel(linearClampSampler, float2( terrain.heightfield_invSize.x, -terrain.heightfield_invSize.y) * smoothness + lowMIPTexCoord, lowMIP).w +
         heightfield.SampleLevel(linearClampSampler, float2(-terrain.heightfield_invSize.x, -terrain.heightfield_invSize.y) * smoothness + lowMIPTexCoord, lowMIP).w +
         heightfield.SampleLevel(linearClampSampler, float2(-terrain.heightfield_invSize.x,  terrain.heightfield_invSize.y) * smoothness + lowMIPTexCoord, lowMIP).w) * 0.25;

    // TODO: Break up very flat surfaces
  }
  float highMIPValue = heightfield.SampleLevel(linearClampSampler, highMIPTexCoord, highMIP).w;
  wsPosition.y = lerp(lowMIPValue, highMIPValue, fractionalLevel);

  // Fold down edges
  float2 distanceFromCenter = abs(lowMIPTexCoord - 0.5);
  if (max(distanceFromCenter.x, distanceFromCenter.y) > 0.5) {
    wsPosition.y = terrain.minElevation - TerrainSkirtDepth;
  }

  // TODO: textures
  float3 color;
  {
    float relativeHeight = (wsPosition.y - terrain.minElevation) / max((terrain.maxElevation - terrain.minElevation), 0.001);

    color = lerp(float3(0.65, 0.57, 0.35), float3(0.19, 0.38, 0.14), smoothstep(0.0, 0.13, relativeHeight));
    color = lerp(color, float3(0.44, 0.34, 0.23), smoothstep(0.22, 0.44, relativeHeight));
    color = lerp(color, float3(0.93, 0.95, 0.97), smoothstep(0.45, 0.78, relativeHeight));
  }

  float4 wsPositionh = float4(wsPosition, 1.0);

  VertexOut output;
  output.position = mul(wsPositionh + float4(args.terrainOrigin.xyz, 0.0), args.viewProjection);
  output.color = color;
  output.terrainXZ = wsPosition.xz;

  return output;
}

#endif
