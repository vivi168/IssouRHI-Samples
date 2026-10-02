#pragma once

#include <compare>
#include <cstdint>
#include <vector>

struct Point {
  float x, y, z;

  auto operator<=>(const Point&) const = default;
};

class TerrainMesh
{
public:
  std::vector<Point> vertices;
  std::vector<std::uint32_t> indices;

  static TerrainMesh BuildMesh();
};
