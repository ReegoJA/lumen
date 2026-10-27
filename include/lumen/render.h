#ifndef LUMEN_RENDER_H_
#define LUMEN_RENDER_H_

#include <string>
#include <vector>

#include "lumen/scene.h"

namespace lumen {
struct Settings {
  int width = 480, height = 480, samples = 64, max_depth = 12, threads = 0;
  uint32_t seed = 1, sample_offset = 0;
  bool mis = true, brute = false;
};
struct Image {
  int width = 0, height = 0;
  std::vector<Vec3> pixels;
};
struct RenderResult {
  Image image;
  double seconds = 0;
  size_t working_bytes = 0;
};
void Validate(const Settings& settings);
RenderResult RenderCpu(const Scene& scene, const Settings& settings);
#ifdef LUMEN_HAS_CUDA
RenderResult RenderCuda(const Scene& scene, const Settings& settings);
#endif
void WritePpm(const Image& image, const std::string& path, float exposure = 0);
void WritePfm(const Image& image, const std::string& path);
Image ReadPfm(const std::string& path);
double Mse(const Image& image, const Image& reference);
std::vector<unsigned char> DisplayPixels(const Image& image, float exposure = 0);
}  // namespace lumen
#endif
