#include "TerrainCommon.hlsli"
#include "TerrainPatch.hlsli"

groupshared TerrainPatchPayload payload;
groupshared uint visiblePatchCount;

bool OutsidePlane(float4 plane, float3 center, float3 extent)
{
  return dot(plane.xyz, center) + plane.w + dot(abs(plane.xyz), extent) < 0.0;
}

bool IsTerrainPatchVisible(uint patchId)
{
  if (patchId >= TerrainMeshPatchCount) {
    return false;
  }

  uint3 patch = DecodeTerrainPatch(patchId);
  uint2 patchExtent = uint2(TerrainMeshPatchWidth, TerrainMeshPatchHeight);
  uint2 firstCell = patch.xy * patchExtent;
  uint2 endCell = min(firstCell + patchExtent, TerrainMeshRingCells);
  int step = int(1u << patch.z);
  int halfExtent = int(TerrainBaseGridExtentTexels >> 1);
  int radius = step * (halfExtent + 1);
  int2 gridMin = int2(firstCell) * step - radius;
  int2 gridMax = int2(endCell) * step - radius;

  // reject patches fully inside the hold at center of climap
  if (patch.z > 0) {
    int prevStep = step >> 1;
    int2 maxDistance = max(abs(gridMin + prevStep), abs(gridMax - step + prevStep));
    if (all(maxDistance < halfExtent * prevStep)) {
      return false;
    }
  }

  // rough camera frustum culling
  StructuredBuffer<TerrainMeasurements> information = ResourceDescriptorHeap[args.terrainInformationId];
  TerrainMeasurements terrain = information[0];
  float2 worldMin = TerrainGridToWorldXZ(float2(gridMin), float(patch.z), terrain);
  float2 worldMax = TerrainGridToWorldXZ(float2(gridMax), float(patch.z), terrain);

  const float heightPadding = 1.0;
  float3 boundsMin = float3(worldMin.x, terrain.minElevation - TerrainSkirtDepth - heightPadding, worldMin.y);
  float3 boundsMax = float3(worldMax.x, terrain.maxElevation + heightPadding, worldMax.y);
  float3 center = (boundsMin + boundsMax) * 0.5 + args.terrainOrigin.xyz;
  float3 extent = (boundsMax - boundsMin) * 0.5 + 0.05; // Roundoff at frustum boundaries.

  // FIXME: transpose once on CPU?
  float4x4 columns = transpose(args.viewProjection);
  return !OutsidePlane(columns[3] + columns[0], center, extent)
      && !OutsidePlane(columns[3] - columns[0], center, extent)
      && !OutsidePlane(columns[3] + columns[1], center, extent)
      && !OutsidePlane(columns[3] - columns[1], center, extent)
      && !OutsidePlane(columns[2], center, extent)
      && !OutsidePlane(columns[3] - columns[2], center, extent);
}

[numthreads(TerrainTaskGroupSize, 1, 1)]
void main(uint3 group : SV_GroupID, uint lane : SV_GroupIndex)
{
  if (lane == 0) {
    visiblePatchCount = 0;
  }
  GroupMemoryBarrierWithGroupSync();

  uint patchId = group.x * TerrainTaskGroupSize + lane;
  bool visible = IsTerrainPatchVisible(patchId);
  uint waveCount = WaveActiveCountBits(visible);
  uint waveOffset = 0;

  if (WaveIsFirstLane()) {
    InterlockedAdd(visiblePatchCount, waveCount, waveOffset);
  }

  waveOffset = WaveReadLaneFirst(waveOffset);
  uint offset = waveOffset + WavePrefixCountBits(visible);

  if (visible) {
    payload.patchIds[offset] = patchId;
  }

  GroupMemoryBarrierWithGroupSync();

  DispatchMesh(visiblePatchCount, 1, 1, payload);
}
