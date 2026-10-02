#ifndef TERRAIN_PATCH_HLSLI
#define TERRAIN_PATCH_HLSLI

#include "../Terrain/Shared.h"

struct TerrainPatchPayload {
  uint patchIds[TerrainTaskGroupSize];
};

uint3 DecodeTerrainPatch(uint id)
{
  return uint3(id % TerrainMeshPatchCountX,
               (id / TerrainMeshPatchCountX) % TerrainMeshPatchCountZ,
               id / (TerrainMeshPatchCountX * TerrainMeshPatchCountZ));
}

#endif
