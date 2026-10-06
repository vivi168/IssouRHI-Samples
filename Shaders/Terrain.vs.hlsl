#include "TerrainCommon.hlsli"

VertexOut main(uint id : SV_VertexID)
{
  StructuredBuffer<float3> vertices = ResourceDescriptorHeap[args.verticesId];
  StructuredBuffer<uint> indices = ResourceDescriptorHeap[args.indicesId];

  return EvaluateTerrainVertex(vertices[indices[id]]);
}
