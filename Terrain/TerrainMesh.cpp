#include "TerrainMesh.h"
#include "Shared.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

struct Mesh {
  std::vector<Point> vertices;
  std::vector<std::uint32_t> indices;
  std::map<Point, std::uint32_t> vertexLookup;

  void Triangle(const Point& a, const Point& b, const Point& c)
  {
    for (const Point& p : {a, b, c}) {
      auto [it, inserted] = vertexLookup.try_emplace(p, static_cast<std::uint32_t>(vertices.size()));

      if (inserted) {
        vertices.push_back(p);
      }

      indices.push_back(it->second);
    }
  }
};

static Point Midpoint(const Point& a, const Point& b)
{
  return {
      .x = (a.x + b.x) * 0.5f,
      .y = (a.y + b.y) * 0.5f,
      .z = (a.z + b.z) * 0.5f,
  };
}

// Adapted from Morgan McGuire, https://casual-effects.blogspot.com/2014/04/fast-terrain-rendering-with-continuous.html
static Mesh BuildRings(int extent, int levels)
{
  Mesh mesh;
  for (int level = 0; level < levels; ++level) {
    const std::int64_t step = std::int64_t{1} << level;
    const std::int64_t prevStep = level == 0 ? 0 : step / 2;
    const std::int64_t g = extent / 2;
    const std::int64_t radius = step * (g + 1);
    const float lod = static_cast<float>(level);

    for (std::int64_t z = -radius; z < radius; z += step) {
      for (std::int64_t x = -radius; x < radius; x += step) {
        if (std::max(std::abs(x + prevStep), std::abs(z + prevStep)) < g * prevStep) {
          continue;
        }

        const Point a{static_cast<float>(x), lod, static_cast<float>(z)};
        const Point c{static_cast<float>(x + step), lod, a.z};
        const Point gPoint{a.x, lod, static_cast<float>(z + step)};
        const Point i{c.x, lod, gPoint.z};
        const Point b = Midpoint(a, c), d = Midpoint(a, gPoint);
        const Point f = Midpoint(c, i), h = Midpoint(gPoint, i);
        const Point e = Midpoint(a, i);

        if (x == -radius) {
          mesh.Triangle(e, a, gPoint);
        } else {
          mesh.Triangle(e, a, d);
          mesh.Triangle(e, d, gPoint);
        }
        if (z == radius - step) {
          mesh.Triangle(e, gPoint, i);
        } else {
          mesh.Triangle(e, gPoint, h);
          mesh.Triangle(e, h, i);
        }
        if (x == radius - step) {
          mesh.Triangle(e, i, c);
        } else {
          mesh.Triangle(e, i, f);
          mesh.Triangle(e, f, c);
        }
        if (z == -radius) {
          mesh.Triangle(e, c, a);
        } else {
          mesh.Triangle(e, c, b);
          mesh.Triangle(e, b, a);
        }
      }
    }
  }
  return mesh;
}

TerrainMesh TerrainMesh::BuildMesh()
{
  auto mesh = BuildRings(static_cast<int>(TerrainBaseGridExtentTexels), static_cast<int>(TerrainMeshLODLevels));

  return {
      .vertices = std::move(mesh.vertices),
      .indices = std::move(mesh.indices),
  };
}
