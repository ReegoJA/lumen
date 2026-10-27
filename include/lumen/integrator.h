#ifndef LUMEN_INTEGRATOR_H_
#define LUMEN_INTEGRATOR_H_

#include "lumen/scene.h"

namespace lumen {
struct Path {
  Ray ray;
  Vec3 throughput{1, 1, 1}, radiance{}, previous_position{};
  Rng rng;
  float previous_pdf = 0;
  bool previous_delta = true, alive = true;
};
struct Shadow {
  Ray ray;
  Vec3 contribution{};
  float maximum = 0;
  bool active = false;
};
LUMEN_HD inline float Fresnel(float cosine, float eta_i, float eta_t) {
  float sin_t = eta_i / eta_t * sqrtf(fmaxf(0, 1 - cosine * cosine));
  if (sin_t >= 1) return 1;
  float cos_t = sqrtf(fmaxf(0, 1 - sin_t * sin_t));
  float a = (eta_t * cosine - eta_i * cos_t) / (eta_t * cosine + eta_i * cos_t);
  float b = (eta_i * cosine - eta_t * cos_t) / (eta_i * cosine + eta_t * cos_t);
  return .5f * (a * a + b * b);
}
LUMEN_HD inline Shadow Shade(SceneView scene, Hit hit, Path& path, int depth, int max_depth,
                             bool mis) {
  Shadow shadow;
  if (hit.primitive < 0) {
    path.alive = false;
    return shadow;
  }
  const Primitive& primitive = scene.primitives[hit.primitive];
  const Material& material = scene.materials[primitive.material];
  bool front = Dot(hit.normal, path.ray.direction) < 0;
  Vec3 n = front ? hit.normal : -hit.normal;
  if (front && MaxComponent(material.emission) > 0) {
    float weight = 1;
    if (mis && !path.previous_delta) {
      float pdf = LightPdf(scene, hit.primitive, path.previous_position, hit.position, hit.normal);
      weight = PowerWeight(path.previous_pdf, pdf);
    }
    path.radiance += path.throughput * material.emission * weight;
  }
  if (depth + 1 >= max_depth) {
    path.alive = false;
    return shadow;
  }
  if (material.surface == Surface::kDiffuse && mis && scene.light_count) {
    int chosen = int(path.rng.Next() * scene.light_count);
    int id = scene.lights[chosen];
    Vec3 normal;
    Vec3 point = SamplePoint(scene.primitives[id], path.rng, normal);
    Vec3 delta = point - hit.position;
    float distance = Length(delta);
    Vec3 direction = delta / distance;
    float cosine = fmaxf(0, Dot(n, direction));
    float pdf = LightPdf(scene, id, hit.position, point, normal);
    if (cosine > 0 && pdf > 0) {
      Vec3 emission = scene.materials[scene.primitives[id].material].emission;
      shadow.ray = {hit.position + direction * kEpsilon, direction};
      shadow.maximum = distance - 2 * kEpsilon;
      shadow.contribution = path.throughput * material.albedo * emission *
                            (cosine / (kPi * pdf) * PowerWeight(pdf, cosine / kPi));
      shadow.active = true;
    }
  }
  Vec3 outgoing;
  path.previous_delta = material.surface != Surface::kDiffuse;
  if (material.surface == Surface::kDiffuse) {
    outgoing = CosineDirection(n, path.rng);
    path.previous_pdf = fmaxf(0, Dot(n, outgoing)) / kPi;
    path.throughput = path.throughput * material.albedo;
  } else if (material.surface == Surface::kMirror) {
    outgoing = Reflect(path.ray.direction, n);
    path.throughput = path.throughput * material.albedo;
  } else {
    float eta_i = front ? 1 : material.ior, eta_t = front ? material.ior : 1;
    float cosine = Clamp(-Dot(path.ray.direction, n), 0, 1), eta = eta_i / eta_t;
    float reflectance = Fresnel(cosine, eta_i, eta_t);
    if (path.rng.Next() < reflectance) {
      outgoing = Reflect(path.ray.direction, n);
    } else {
      float sin2 = eta * eta * (1 - cosine * cosine);
      outgoing =
          Normalize(path.ray.direction * eta + n * (eta * cosine - sqrtf(fmaxf(0, 1 - sin2))));
      path.throughput = path.throughput * (eta * eta);
    }
    path.throughput = path.throughput * material.albedo;
  }
  path.previous_position = hit.position;
  path.ray = {hit.position + outgoing * kEpsilon, outgoing};
  if (depth >= 3) {
    float survival = Clamp(MaxComponent(path.throughput), .05f, .95f);
    if (path.rng.Next() >= survival)
      path.alive = false;
    else
      path.throughput = path.throughput / survival;
  }
  return shadow;
}
}  // namespace lumen
#endif
