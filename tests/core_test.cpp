#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "lumen/integrator.h"
#include "lumen/render.h"

namespace {
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void Geometry() {
  lumen::Hit hit;
  lumen::Primitive sphere{{0, 0, 0}, {}, {}, 1, 0};
  Require(lumen::IntersectPrimitive(sphere, {{0, 0, 3}, {0, 0, -1}}, hit, 0), "Sphere miss");
  Require(std::abs(hit.distance - 2) < 1e-6, "Incorrect sphere root");
  hit = {};
  Require(lumen::IntersectPrimitive(sphere, {{0, 0, 0}, {1, 0, 0}}, hit, 0), "Inside sphere miss");
  Require(std::abs(hit.distance - 1) < 1e-6, "Incorrect inside sphere root");
  lumen::Bounds box{{-1, -1, -1}, {1, 1, 1}};
  Require(box.Intersects({{1, 0, 3}, {0, 0, -1}}, 10), "Parallel slab boundary miss");
  Require(!box.Intersects({{2, 0, 3}, {0, 0, -1}}, 10), "Parallel slab false hit");
  auto scene = lumen::MakeScene("spheres");
  lumen::Rng rng(72);
  for (int i = 0; i < 20000; ++i) {
    lumen::Ray ray{{4 * rng.Next() - 2, 3 * rng.Next(), 4 * rng.Next() - 2},
                   lumen::Normalize({2 * rng.Next() - 1, 2 * rng.Next() - 1, 2 * rng.Next() - 1})};
    auto a = lumen::Intersect(scene.View(), ray),
         b = lumen::Intersect(scene.View(), ray, lumen::kInfinity, true);
    Require((a.primitive < 0) == (b.primitive < 0), "BVH hit disagreement");
    if (a.primitive >= 0)
      Require(std::abs(a.distance - b.distance) < 1e-5, "BVH distance disagreement");
  }
}
void Sampling() {
  lumen::Rng rng(19);
  double cosine = 0;
  for (int i = 0; i < 100000; ++i) {
    auto v = lumen::CosineDirection({0, 1, 0}, rng);
    Require(v.y >= 0 && std::abs(lumen::Length(v) - 1) < 1e-5, "Invalid hemisphere sample");
    cosine += v.y;
  }
  Require(std::abs(cosine / 100000 - 2.0 / 3) < .003, "Cosine sampling moment");
  Require(std::abs(lumen::Fresnel(1, 1, 1.5f) - .04f) < 1e-6, "Normal incidence Fresnel");
  Require(lumen::Fresnel(.1f, 1.5f, 1) == 1, "Total internal reflection");
  Require(std::abs(lumen::PowerWeight(.3f, .7f) + lumen::PowerWeight(.7f, .3f) - 1) < 1e-6,
          "MIS partition of unity");
}
void Rendering() {
  auto scene = lumen::MakeScene("cornell");
  lumen::Settings s;
  s.width = 24;
  s.height = 24;
  s.samples = 32;
  s.threads = 1;
  auto first = lumen::RenderCpu(scene, s).image;
  s.threads = 4;
  auto threaded = lumen::RenderCpu(scene, s).image;
  Require(lumen::Mse(first, threaded) == 0, "Thread count changes image");
  s.brute = true;
  auto brute = lumen::RenderCpu(scene, s).image;
  Require(lumen::Mse(first, brute) < 1e-10, "BVH changes image");
  double energy = 0;
  for (auto p : first.pixels)
    for (int c = 0; c < 3; ++c) {
      Require(std::isfinite(p[c]) && p[c] >= 0, "Invalid radiance");
      energy += p[c];
    }
  Require(energy > 1, "Black render");
  auto file = std::filesystem::temp_directory_path() / "lumen-test.pfm";
  lumen::WritePfm(first, file.string());
  Require(lumen::Mse(first, lumen::ReadPfm(file.string())) == 0, "PFM roundtrip");
  std::filesystem::remove(file);
#ifdef LUMEN_HAS_CUDA
  if (std::getenv("LUMEN_TEST_GPU")) {
    s.brute = false;
    auto gpu = lumen::RenderCuda(scene, s).image;
    double relative = lumen::Mse(first, gpu) / (energy / first.pixels.size());
    Require(relative < 1e-4, "CPU GPU image mismatch");
    std::cout << "CPU GPU normalized MSE: " << relative << '\n';
  }
#endif
}
void EstimatorAgreement() {
  auto scene = lumen::MakeScene("cornell");
  lumen::Settings settings;
  settings.width = settings.height = 24;
  settings.samples = 2048;
  settings.threads = 4;
  settings.seed = 31;
  auto mis = lumen::RenderCpu(scene, settings).image;
  settings.mis = false;
  settings.seed = 47;
  auto bsdf = lumen::RenderCpu(scene, settings).image;
  for (int channel = 0; channel < 3; ++channel) {
    double a = 0, b = 0;
    for (size_t i = 0; i < mis.pixels.size(); ++i) {
      a += mis.pixels[i][channel];
      b += bsdf.pixels[i][channel];
    }
    Require(std::abs(a - b) / a < .1, "MIS and BSDF energy disagreement");
  }
}
}  // namespace
int main() {
  try {
    Geometry();
    Sampling();
    Rendering();
    EstimatorAgreement();
    std::cout << "Geometry, sampling, determinism, traversal, and image tests passed.\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
