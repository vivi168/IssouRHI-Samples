#include "../Terrain/Shared.h"

ConstantBuffer<TerrainMipArgs> args : register(b0);

// Adapted from Morgan McGuire, https://casual-effects.blogspot.com/2014/04/fast-terrain-rendering-with-continuous.html
[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
  if (id.x >= args.width || id.y >= args.height) {
    return;
  }

  Texture2D<float4> source = ResourceDescriptorHeap[args.sourceId];
  RWTexture2D<float4> destination = ResourceDescriptorHeap[args.destinationId];

  static const float weight[9] = {
      1.0 / 16.0,  1.0 / 8.0,  1.0 / 16.0,
      1.0 /  8.0,  1.0 / 4.0,  1.0 /  8.0,
      1.0 / 16.0,  1.0 / 8.0,  1.0 / 16.0
  };

  uint width, height;
  source.GetDimensions(width, height);
  float4 normalAndElevation = 0;

  int2 center = id.xy * 2;

  // Clamp to the image bounds
  int2 sz = int2(width, height) - 1;

  int i = 0;
  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x, ++i) {
      int2 p = clamp(center + int2(x, y), int2(0, 0), sz);
      normalAndElevation += source.Load(int3(p, 0)) * weight[i];
    }
  }

  destination[id.xy] = normalAndElevation;
}
