#include "TerrainCommon.hlsli"

VertexOut VSMain(uint id : SV_VertexID)
{
  StructuredBuffer<float3> vertices = ResourceDescriptorHeap[args.verticesId];
  StructuredBuffer<uint> indices = ResourceDescriptorHeap[args.indicesId];
  return EvaluateTerrainVertex(vertices[indices[id]]);
}

float4 PSMain(VertexOut input) : SV_Target0
{
  SamplerState linearClampSampler = SamplerDescriptorHeap[args.samplerId];
  StructuredBuffer<TerrainMeasurements> information = ResourceDescriptorHeap[args.terrainInformationId];
  TerrainMeasurements terrain = information[0];
  Texture2D<float4> heightfield = ResourceDescriptorHeap[args.heightfieldId];
  // Use LOD0 normal
  float2 texCoord = (input.terrainXZ * terrain.heightfieldTexelsPerMeter + 0.5) * terrain.heightfield_invSize;
  float3 normal = normalize(heightfield.SampleLevel(linearClampSampler, texCoord, 0.0).xyz);
  float3 lightDirection = normalize(float3(-0.45, 0.8, 0.35));
  float lighting = 0.35 + 0.8 * saturate(dot(normal, lightDirection));

  return float4(input.color * lighting, 1.0);
}
