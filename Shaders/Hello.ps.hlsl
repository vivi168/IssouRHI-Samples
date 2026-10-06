#include "HelloCommon.hlsli"

float4 main(VertexOut input) : SV_Target0
{
  return input.color;
}
