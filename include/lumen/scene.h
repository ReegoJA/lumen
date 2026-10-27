#ifndef LUMEN_SCENE_H_
#define LUMEN_SCENE_H_

#include <string>
#include <vector>

#include "lumen/math.h"

namespace lumen {
enum class Surface : int { kDiffuse, kMirror, kGlass };
struct Material {
  Vec3 albedo{.75f, .75f, .75f}, emission{};
  Surface surface = Surface::kDiffuse;
  float ior = 1.5f;
};
struct Primitive {
  Vec3 a, b, c;
  float radius = 0;
  int material = 0;
  LUMEN_HD bool IsSphere() const { return radius > 0; }
  LUMEN_HD float Area() const {
    return IsSphere() ? 4 * kPi * radius * radius : .5f * Length(Cross(b - a, c - a));
  }
};
struct Hit {
  float distance = kInfinity;
  Vec3 position{}, normal{};
  int primitive = -1;
};
struct Node {
  Bounds bounds;
  int first = 0, count = 0, right = 0;
};
struct SceneView {
  const Primitive* primitives;
  const Material* materials;
  const Node* nodes;
  const int* indices;
  const int* lights;
  int primitive_count, node_count, light_count;
};
struct Camera {
  Vec3 position{0, 1, 3.6f}, target{0, 1, 0};
  float vertical_fov = 39;
  LUMEN_HD Ray Generate(int x, int y, int width, int height, Rng& rng) const {
    Vec3 forward = Normalize(target - position);
    Vec3 right = Normalize(Cross(forward, {0, 1, 0})), up = Cross(right, forward);
    float scale = tanf(vertical_fov * kPi / 360);
    float sx = (2 * (x + rng.Next()) / width - 1) * scale * width / height;
    float sy = (1 - 2 * (y + rng.Next()) / height) * scale;
    return {position, Normalize(forward + right * sx + up * sy)};
  }
};
struct Scene {
  std::vector<Primitive> primitives;
  std::vector<Material> materials;
  std::vector<Node> nodes;
  std::vector<int> indices, lights;
  Camera camera;
  void Build();
  SceneView View() const;
};
Scene MakeScene(const std::string& name);

LUMEN_HD inline bool IntersectPrimitive(const Primitive& p, Ray ray, Hit& hit, int id) {
  float distance;
  Vec3 normal;
  if (p.IsSphere()) {
    Vec3 offset = ray.origin - p.a;
    float a = Dot(ray.direction, ray.direction), b = Dot(offset, ray.direction);
    float c = Dot(offset, offset) - p.radius * p.radius;
    float discriminant = b * b - a * c;
    if (discriminant < 0) return false;
    float root = sqrtf(discriminant);
    distance = (-b - root) / a;
    if (distance <= kEpsilon) distance = (-b + root) / a;
    if (distance <= kEpsilon || distance >= hit.distance) return false;
    normal = Normalize(ray.origin + ray.direction * distance - p.a);
  } else {
    Vec3 edge1 = p.b - p.a, edge2 = p.c - p.a;
    Vec3 cross = Cross(ray.direction, edge2);
    float determinant = Dot(edge1, cross);
    if (fabsf(determinant) < 1e-8f) return false;
    float inv = 1 / determinant;
    Vec3 offset = ray.origin - p.a;
    float u = Dot(offset, cross) * inv;
    if (u < 0 || u > 1) return false;
    Vec3 q = Cross(offset, edge1);
    float v = Dot(ray.direction, q) * inv;
    if (v < 0 || u + v > 1) return false;
    distance = Dot(edge2, q) * inv;
    if (distance <= kEpsilon || distance >= hit.distance) return false;
    normal = Normalize(Cross(edge1, edge2));
  }
  hit = {distance, ray.origin + ray.direction * distance, normal, id};
  return true;
}
LUMEN_HD inline Hit Intersect(SceneView scene, Ray ray, float maximum = kInfinity,
                              bool brute = false) {
  Hit hit;
  hit.distance = maximum;
  if (brute) {
    for (int i = 0; i < scene.primitive_count; ++i)
      IntersectPrimitive(scene.primitives[i], ray, hit, i);
    return hit;
  }
  int stack[64], size = 0;
  if (scene.node_count) stack[size++] = 0;
  while (size) {
    int index = stack[--size];
    const Node& node = scene.nodes[index];
    if (!node.bounds.Intersects(ray, hit.distance)) continue;
    if (node.count) {
      for (int i = 0; i < node.count; ++i) {
        int id = scene.indices[node.first + i];
        IntersectPrimitive(scene.primitives[id], ray, hit, id);
      }
    } else {
      stack[size++] = node.right;
      stack[size++] = index + 1;
    }
  }
  return hit;
}
LUMEN_HD inline Vec3 SamplePoint(const Primitive& light, Rng& rng, Vec3& normal) {
  if (light.IsSphere()) {
    float z = 1 - 2 * rng.Next(), phi = 2 * kPi * rng.Next(), r = sqrtf(fmaxf(0, 1 - z * z));
    normal = {r * cosf(phi), r * sinf(phi), z};
    return light.a + normal * light.radius;
  }
  float s = sqrtf(rng.Next()), t = rng.Next();
  normal = Normalize(Cross(light.b - light.a, light.c - light.a));
  return light.a * (1 - s) + light.b * (s * (1 - t)) + light.c * (s * t);
}
LUMEN_HD inline float LightPdf(SceneView scene, int id, Vec3 from, Vec3 position, Vec3 normal) {
  Vec3 delta = position - from;
  float distance2 = Dot(delta, delta);
  float cosine = Dot(normal, -Normalize(delta));
  if (cosine <= 0 || !scene.light_count) return 0;
  return distance2 / (cosine * scene.primitives[id].Area() * scene.light_count);
}
}  // namespace lumen
#endif
