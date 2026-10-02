#ifndef TERRAIN_SHARED_H
#define TERRAIN_SHARED_H
#ifdef __cplusplus
#include <DirectXMath.h>
#include <cstdint>
using hlsl_float4x4 = DirectX::XMFLOAT4X4;
using hlsl_float4 = DirectX::XMFLOAT4;
using hlsl_float2 = DirectX::XMFLOAT2;
using hlsl_uint = std::uint32_t;
#define ASSERT_SIZE_M16(T) static_assert(sizeof(T) % 16 == 0, #T " size must be multiple of 16")
#define ASSERT_SIZE_U32(T) static_assert(sizeof(T) % sizeof(hlsl_uint) == 0, #T " size must be multiple of UINT32")
#define HLSL_CONST inline constexpr
#else
#define hlsl_float4x4 float4x4
#define hlsl_float4 float4
#define hlsl_float2 float2
#define hlsl_uint uint
#define ASSERT_SIZE_M16(T)
#define ASSERT_SIZE_U32(T)
#define HLSL_CONST static const
#endif

HLSL_CONST hlsl_uint TerrainHeightfieldSize = 4096;
HLSL_CONST hlsl_uint TerrainBaseGridExtentTexels = 128;
HLSL_CONST hlsl_uint TerrainMeshLODLevels = 7;
HLSL_CONST hlsl_uint TerrainMeshPatchWidth = 3;
HLSL_CONST hlsl_uint TerrainMeshPatchHeight = 4;
HLSL_CONST hlsl_uint TerrainMeshRingCells = 2 * (TerrainBaseGridExtentTexels / 2 + 1);
HLSL_CONST hlsl_uint TerrainMeshPatchCountX = (TerrainMeshRingCells + TerrainMeshPatchWidth - 1) / TerrainMeshPatchWidth;
HLSL_CONST hlsl_uint TerrainMeshPatchCountZ = (TerrainMeshRingCells + TerrainMeshPatchHeight - 1) / TerrainMeshPatchHeight;
HLSL_CONST hlsl_uint TerrainMeshPatchCount = TerrainMeshPatchCountX * TerrainMeshPatchCountZ * TerrainMeshLODLevels;
HLSL_CONST hlsl_uint TerrainTaskGroupSize = 32;
HLSL_CONST float TerrainSkirtDepth = 100.0;
HLSL_CONST hlsl_uint TerrainHeightfieldMipCount = TerrainMeshLODLevels + 1;
HLSL_CONST float TerrainInvBaseGridSizeTexels = 1.0 / TerrainBaseGridExtentTexels;

struct TerrainMeasurements {
  float metersPerTexel;
  float minElevation;
  float maxElevation;
  float heightfieldTexelsPerMeter;
  hlsl_float2 heightfield_invSize;
};
ASSERT_SIZE_U32(TerrainMeasurements);

struct TerrainDrawArgs {
  hlsl_float4x4 viewProjection;
  hlsl_float4 terrainOrigin;  // xyz: terrain world position minus camera world position
  hlsl_uint verticesId;
  hlsl_uint indicesId;
  hlsl_uint heightfieldId;
  hlsl_uint terrainInformationId;
  hlsl_uint samplerId;
};
ASSERT_SIZE_U32(TerrainDrawArgs);

struct TerrainGenerateArgs {
  hlsl_uint sourceId;
  hlsl_uint destinationId;
  hlsl_uint terrainInformationId;
};
ASSERT_SIZE_U32(TerrainGenerateArgs);

struct TerrainMipArgs {
  hlsl_uint sourceId;
  hlsl_uint destinationId;
  hlsl_uint width;
  hlsl_uint height;
};
ASSERT_SIZE_U32(TerrainMipArgs);

#endif
