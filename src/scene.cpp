#include "lumen/scene.h"

#include <algorithm>
#include <functional>
#include <numeric>
#include <stdexcept>

namespace lumen {
namespace {
Bounds PrimitiveBounds(const Primitive& p) {
  if (p.IsSphere()) {
    Vec3 r{p.radius, p.radius, p.radius};
    return {p.a - r, p.a + r};
  }
  return {Min(p.a, Min(p.b, p.c)), Max(p.a, Max(p.b, p.c))};
}
void Quad(Scene& s, Vec3 a, Vec3 b, Vec3 c, Vec3 d, int material) {
  s.primitives.push_back({a, b, c, 0, material});
  s.primitives.push_back({a, c, d, 0, material});
}
void Box(Scene& s, Vec3 center, Vec3 half, float angle, int material) {
  Vec3 points[8];
  for (int i = 0; i < 8; ++i) {
    Vec3 p{(i & 1) ? half.x : -half.x, (i & 2) ? half.y : -half.y, (i & 4) ? half.z : -half.z};
    points[i] = center + Vec3{cosf(angle) * p.x + sinf(angle) * p.z, p.y,
                              -sinf(angle) * p.x + cosf(angle) * p.z};
  }
  const int faces[6][4] = {{0, 4, 6, 2}, {1, 3, 7, 5}, {0, 1, 5, 4},
                           {2, 6, 7, 3}, {0, 2, 3, 1}, {4, 5, 7, 6}};
  for (const auto& f : faces)
    Quad(s, points[f[0]], points[f[1]], points[f[2]], points[f[3]], material);
}
}  // namespace

void Scene::Build() {
  if (primitives.empty() || materials.empty()) throw std::invalid_argument("Empty scene");
  lights.clear();
  nodes.clear();
  indices.resize(primitives.size());
  std::iota(indices.begin(), indices.end(), 0);
  for (size_t i = 0; i < primitives.size(); ++i) {
    const auto& p = primitives[i];
    if (p.material < 0 || p.material >= int(materials.size()) || p.Area() <= 0)
      throw std::invalid_argument("Invalid primitive");
    if (MaxComponent(materials[p.material].emission) > 0) lights.push_back(int(i));
  }
  std::function<int(int, int, int)> build = [&](int first, int count, int depth) {
    if (depth >= 60) throw std::runtime_error("BVH exceeds traversal stack capacity");
    int index = int(nodes.size());
    nodes.push_back({});
    Bounds bounds, centroids;
    for (int i = first; i < first + count; ++i) {
      Bounds b = PrimitiveBounds(primitives[indices[i]]);
      bounds.lo = Min(bounds.lo, b.lo);
      bounds.hi = Max(bounds.hi, b.hi);
      Vec3 c = (b.lo + b.hi) * .5f;
      centroids.lo = Min(centroids.lo, c);
      centroids.hi = Max(centroids.hi, c);
    }
    nodes[index].bounds = bounds;
    if (count <= 4) {
      nodes[index].first = first;
      nodes[index].count = count;
      return index;
    }
    Vec3 extent = centroids.hi - centroids.lo;
    int axis = extent.x > extent.y ? 0 : 1;
    if (extent.z > extent[axis]) axis = 2;
    int middle = first + count / 2;
    std::nth_element(indices.begin() + first, indices.begin() + middle,
                     indices.begin() + first + count, [&](int a, int b) {
                       Bounds aa = PrimitiveBounds(primitives[a]),
                              bb = PrimitiveBounds(primitives[b]);
                       float ca = (aa.lo[axis] + aa.hi[axis]), cb = (bb.lo[axis] + bb.hi[axis]);
                       return ca == cb ? a < b : ca < cb;
                     });
    build(first, count / 2, depth + 1);
    int right = build(middle, count - count / 2, depth + 1);
    nodes[index].right = right;
    return index;
  };
  build(0, int(primitives.size()), 0);
}
SceneView Scene::View() const {
  return {primitives.data(), materials.data(),       nodes.data(),      indices.data(),
          lights.data(),     int(primitives.size()), int(nodes.size()), int(lights.size())};
}
Scene MakeScene(const std::string& name) {
  if (name != "cornell" && name != "glass" && name != "spheres")
    throw std::invalid_argument("Scene must be cornell, glass, or spheres");
  Scene s;
  s.materials = {
      {{.76f, .76f, .72f}, {}, Surface::kDiffuse}, {{.65f, .08f, .05f}, {}, Surface::kDiffuse},
      {{.08f, .35f, .55f}, {}, Surface::kDiffuse}, {{0, 0, 0}, {16, 14, 11}, Surface::kDiffuse},
      {{.94f, .94f, .94f}, {}, Surface::kMirror},  {{1, 1, 1}, {}, Surface::kGlass, 1.5f},
      {{.72f, .45f, .12f}, {}, Surface::kDiffuse}};
  Quad(s, {-1, 0, 1}, {1, 0, 1}, {1, 0, -1}, {-1, 0, -1}, 0);
  Quad(s, {-1, 2, -1}, {1, 2, -1}, {1, 2, 1}, {-1, 2, 1}, 0);
  Quad(s, {-1, 0, -1}, {1, 0, -1}, {1, 2, -1}, {-1, 2, -1}, 0);
  Quad(s, {-1, 0, 1}, {-1, 0, -1}, {-1, 2, -1}, {-1, 2, 1}, 1);
  Quad(s, {1, 0, -1}, {1, 0, 1}, {1, 2, 1}, {1, 2, -1}, 2);
  Quad(s, {-.35f, 1.99f, -.35f}, {.35f, 1.99f, -.35f}, {.35f, 1.99f, .25f}, {-.35f, 1.99f, .25f},
       3);
  if (name == "cornell") {
    Box(s, {-.43f, .3f, .25f}, {.27f, .3f, .27f}, -.25f, 0);
    Box(s, {.37f, .61f, -.34f}, {.27f, .61f, .27f}, .32f, 0);
  } else if (name == "glass") {
    s.primitives.push_back({{-.43f, .4f, .15f}, {}, {}, .4f, 5});
    s.primitives.push_back({{.4f, .4f, -.3f}, {}, {}, .4f, 4});
    Box(s, {0, .08f, .45f}, {.15f, .08f, .13f}, .2f, 6);
  } else {
    for (int y = 0; y < 6; ++y)
      for (int z = 0; z < 9; ++z)
        for (int x = 0; x < 9; ++x) {
          s.primitives.push_back({{-.8f + x * .2f, .07f + y * .18f, -.8f + z * .2f},
                                  {},
                                  {},
                                  .065f,
                                  (x + y + z) % 7 == 0   ? 4
                                  : (x + y + z) % 3 == 0 ? 6
                                                         : 0});
        }
    s.camera.position = {0, 1.35f, 4};
  }
  s.Build();
  return s;
}
}  // namespace lumen
