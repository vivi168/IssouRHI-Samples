#include "../Terrain/Shared.h"

ConstantBuffer<TerrainGenerateArgs> args : register(b0);

// Adapted from Morgan McGuire, https://casual-effects.blogspot.com/2014/04/fast-terrain-rendering-with-continuous.html
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
  StructuredBuffer<TerrainMeasurements> information = ResourceDescriptorHeap[args.terrainInformationId];
  TerrainMeasurements terrain = information[0];
  if (id.x >= TerrainHeightfieldSize || id.y >= TerrainHeightfieldSize) {
    return;
  }

  int2 center = id.xy;

  Texture2D<float> source = ResourceDescriptorHeap[args.sourceId];
  RWTexture2D<float4> destination = ResourceDescriptorHeap[args.destinationId];
  float elevation = source.Load(int3(id.xy, 0)) * (terrain.maxElevation - terrain.minElevation) + terrain.minElevation;

  // Radius in fine texels over which we average normals along axes
  static const int r = 4;

  // Radius in fine texels over which we average normals orthogonal to the axes
  static const int s = 2;

  const int2 maxCoord = int2(TerrainHeightfieldSize, TerrainHeightfieldSize) - 1;

  // Average height difference
  float2 delta = 0.0;

  float weightSum = 0.0;
  for (int shift = -s; shift < +s; ++shift) {
    float weight = 1.0 / (abs(shift) + 1.0);
    weightSum += weight;

    for (int d = 1; d <= r; ++d) {
      int2 xPositive = clamp(center + int2(d, shift), 0, maxCoord);
      int2 xNegative = clamp(center - int2(d, shift), 0, maxCoord);
      int2 zPositive = clamp(center + int2(shift, d), 0, maxCoord);
      int2 zNegative = clamp(center - int2(shift, d), 0, maxCoord);

      delta.x += (source.Load(int3(xPositive, 0)) - source.Load(int3(xNegative, 0))) * weight;
      delta.y += (source.Load(int3(zPositive, 0)) - source.Load(int3(zNegative, 0))) * weight;
    }
  }

  delta *= (terrain.maxElevation - terrain.minElevation) / (float(r) * weightSum);
  float span = 2.0 * terrain.metersPerTexel * r;
  float3 normal = normalize(cross(float3(0.0, delta.y, span), float3(span, delta.x, 0.0)));

  destination[id.xy] = float4(normal, elevation);
}
