#include "lumen/render.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

#include "lumen/integrator.h"

namespace lumen {
void Validate(const Settings& s) {
  if (s.width < 1 || s.height < 1 || s.width > 8192 || s.height > 8192 || s.samples < 1 ||
      s.samples > 1000000 || s.max_depth < 1 || s.max_depth > 64 || s.threads < 0 ||
      s.threads > 1024)
    throw std::invalid_argument("Invalid image, sample, depth, or thread count");
  if (uint64_t(s.sample_offset) + s.samples > UINT32_MAX)
    throw std::invalid_argument("Sample index overflow");
}
RenderResult RenderCpu(const Scene& scene, const Settings& s) {
  Validate(s);
  auto start = std::chrono::steady_clock::now();
  RenderResult result;
  result.image = {s.width, s.height, std::vector<Vec3>(size_t(s.width) * s.height)};
  int threads = s.threads ? s.threads : int(std::thread::hardware_concurrency());
  threads = std::max(1, std::min(threads, s.height));
  std::atomic<int> next_row{0};
  SceneView view = scene.View();
  auto worker = [&]() {
    while (true) {
      int y = next_row.fetch_add(1);
      if (y >= s.height) break;
      for (int x = 0; x < s.width; ++x) {
        int pixel = y * s.width + x;
        Vec3 sum;
        for (int sample = 0; sample < s.samples; ++sample) {
          Path path;
          path.rng = Rng(Seed(pixel, sample + s.sample_offset, s.seed));
          path.ray = scene.camera.Generate(x, y, s.width, s.height, path.rng);
          for (int depth = 0; depth < s.max_depth && path.alive; ++depth) {
            Hit hit = Intersect(view, path.ray, kInfinity, s.brute);
            Shadow shadow = Shade(view, hit, path, depth, s.max_depth, s.mis);
            if (shadow.active && Intersect(view, shadow.ray, shadow.maximum, s.brute).primitive < 0)
              path.radiance += shadow.contribution;
          }
          sum += path.radiance;
        }
        result.image.pixels[pixel] = sum / float(s.samples);
      }
    }
  };
  std::vector<std::thread> workers;
  for (int i = 1; i < threads; ++i) workers.emplace_back(worker);
  worker();
  for (auto& thread : workers) thread.join();
  result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  result.working_bytes =
      result.image.pixels.size() * sizeof(Vec3) + scene.nodes.size() * sizeof(Node) +
      scene.primitives.size() * sizeof(Primitive) + scene.materials.size() * sizeof(Material) +
      (scene.indices.size() + scene.lights.size()) * sizeof(int) + threads * sizeof(Path);
  return result;
}
}  // namespace lumen
