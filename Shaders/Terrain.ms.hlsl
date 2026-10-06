#include "TerrainCommon.hlsli"
#include "TerrainPatch.hlsli"

// A 3x4 patch of the existing eight-triangle cells shares a 7x9 vertex grid:
// 63 vertices and at most 96 triangles (as per nvidia guidelines: < 64 vertices and < 126 triangles)
static const uint PatchCells = TerrainMeshPatchWidth * TerrainMeshPatchHeight;
static const uint PatchVertexWidth = 2 * TerrainMeshPatchWidth + 1;
static const uint PatchVertexCount = PatchVertexWidth * (2 * TerrainMeshPatchHeight + 1);
static const uint PatchTriangleCount = 8 * PatchCells;
static const uint MeshGroupSize = 32;
groupshared uint cellTriangleCounts[PatchCells];

[outputtopology("triangle")]
[numthreads(MeshGroupSize, 1, 1)]
void main(uint3 group : SV_GroupID,
          uint lane : SV_GroupIndex,
#ifdef TERRAIN_USE_TASK_SHADER
          in payload TerrainPatchPayload payload,
#endif
          out vertices VertexOut vertices[PatchVertexCount],
          out indices uint3 triangles[PatchTriangleCount])
{
#ifdef TERRAIN_USE_TASK_SHADER
  group = DecodeTerrainPatch(payload.patchIds[group.x]);
#endif
  uint2 patchOrigin = group.xy * uint2(TerrainMeshPatchWidth, TerrainMeshPatchHeight);
  int step = int(1u << group.z);
  int prevStep = group.z == 0 ? 0 : step >> 1;
  int halfExtent = int(TerrainBaseGridExtentTexels >> 1);
  int radius = step * (halfExtent + 1);
  uint2 cell = patchOrigin + uint2(lane % TerrainMeshPatchWidth, lane / TerrainMeshPatchWidth);
  int2 cellPosition = int2(cell) * step - radius;
  bool4 border = bool4(cell.x == 0, cell.y == TerrainMeshRingCells - 1, cell.x == TerrainMeshRingCells - 1, cell.y == 0);

  // Match TerrainMesh::BuildMesh's inner hole and stitched outer edges.
  if (lane < PatchCells) {
    int2 distance = abs(cellPosition + prevStep);
    bool active = all(cell < TerrainMeshRingCells) && max(distance.x, distance.y) >= halfExtent * prevStep;

    cellTriangleCounts[lane] = active ? 8 - uint(border.x) - uint(border.y) - uint(border.z) - uint(border.w) : 0;
  }

  GroupMemoryBarrierWithGroupSync();

  uint triangleCount = 0;
  uint triangleOffset = 0;
  [unroll]
  for (uint i = 0; i < PatchCells; ++i) {
    triangleCount += cellTriangleCounts[i];
    if (i < lane) {
      triangleOffset += cellTriangleCounts[i];
    }
  }

  // always emit 63 vertices for simplicity sake unless no triangle, in which case 0 vertices
  SetMeshOutputCounts(triangleCount == 0 ? 0 : PatchVertexCount, triangleCount);

  if (triangleCount == 0) {
    return;
  }

  for (uint v = lane; v < PatchVertexCount; v += MeshGroupSize) {
    uint2 local = uint2(v % PatchVertexWidth, v / PatchVertexWidth);
    float2 position = float2(patchOrigin * 2 + local) * (0.5 * step) - radius;

    vertices[v] = EvaluateTerrainVertex(float3(position.x, float(group.z), position.y));
  }

  if (lane < PatchCells) {
    if (cellTriangleCounts[lane] != 0) {
      uint a = 2 * (lane / TerrainMeshPatchWidth) * PatchVertexWidth + 2 * (lane % TerrainMeshPatchWidth);
      uint b = a + 1;
      uint c = a + 2;
      uint d = a + PatchVertexWidth;
      uint e = d + 1;
      uint f = d + 2;
      uint g = d + PatchVertexWidth;
      uint h = g + 1;
      uint i = g + 2;

      if (border.x) {
        triangles[triangleOffset++] = uint3(e, a, g);
      } else {
        triangles[triangleOffset++] = uint3(e, a, d);
        triangles[triangleOffset++] = uint3(e, d, g);
      }
      if (border.y) {
        triangles[triangleOffset++] = uint3(e, g, i);
      } else {
        triangles[triangleOffset++] = uint3(e, g, h);
        triangles[triangleOffset++] = uint3(e, h, i);
      }
      if (border.z) {
        triangles[triangleOffset++] = uint3(e, i, c);
      } else {
        triangles[triangleOffset++] = uint3(e, i, f);
        triangles[triangleOffset++] = uint3(e, f, c);
      }
      if (border.w) {
        triangles[triangleOffset++] = uint3(e, c, a);
      } else {
        triangles[triangleOffset++] = uint3(e, c, b);
        triangles[triangleOffset++] = uint3(e, b, a);
      }
    }
  }
}
