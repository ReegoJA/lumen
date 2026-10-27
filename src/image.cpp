#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "lumen/render.h"

namespace lumen {
std::vector<unsigned char> DisplayPixels(const Image& image, float exposure) {
  std::vector<unsigned char> bytes(image.pixels.size() * 3);
  float gain = std::exp2(exposure);
  for (size_t i = 0; i < image.pixels.size(); ++i)
    for (int c = 0; c < 3; ++c) {
      float value = image.pixels[i][c] * gain;
      value = std::max(0.0f, value);
      value = value / (1 + value);
      float srgb = value <= .0031308f ? 12.92f * value : 1.055f * std::pow(value, 1 / 2.4f) - .055f;
      bytes[3 * i + c] = static_cast<unsigned char>(255 * Clamp(srgb, 0, 1) + .5f);
    }
  return bytes;
}
void WritePpm(const Image& image, const std::string& path, float exposure) {
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("Cannot open output: " + path);
  auto bytes = DisplayPixels(image, exposure);
  out << "P6\n" << image.width << ' ' << image.height << "\n255\n";
  out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
  if (!out) throw std::runtime_error("Image write failed");
}
void WritePfm(const Image& image, const std::string& path) {
  uint16_t endian = 1;
  bool little = *reinterpret_cast<unsigned char*>(&endian) == 1;
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("Cannot open output: " + path);
  out << "PF\n" << image.width << ' ' << image.height << (little ? "\n-1.0\n" : "\n1.0\n");
  for (int y = image.height - 1; y >= 0; --y)
    for (int x = 0; x < image.width; ++x) {
      Vec3 p = image.pixels[size_t(y) * image.width + x];
      float channels[3] = {p.x, p.y, p.z};
      out.write(reinterpret_cast<const char*>(channels), sizeof(channels));
    }
  if (!out) throw std::runtime_error("Image write failed");
}
Image ReadPfm(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::string magic;
  Image image;
  float scale;
  if (!(in >> magic >> image.width >> image.height >> scale) || magic != "PF" || scale == 0 ||
      !std::isfinite(scale) || image.width < 1 || image.height < 1 || image.width > 8192 ||
      image.height > 8192)
    throw std::runtime_error("Invalid PFM header");
  in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  image.pixels.resize(size_t(image.width) * image.height);
  uint16_t endian = 1;
  bool swap = (scale < 0) != (*reinterpret_cast<unsigned char*>(&endian) == 1);
  for (int y = image.height - 1; y >= 0; --y)
    for (int x = 0; x < image.width; ++x) {
      float channels[3];
      in.read(reinterpret_cast<char*>(channels), sizeof(channels));
      if (!in) throw std::runtime_error("Truncated PFM");
      for (float& value : channels) {
        if (swap)
          std::reverse(reinterpret_cast<char*>(&value), reinterpret_cast<char*>(&value) + 4);
        value *= std::abs(scale);
        if (!std::isfinite(value)) throw std::runtime_error("Nonfinite PFM value");
      }
      image.pixels[size_t(y) * image.width + x] = {channels[0], channels[1], channels[2]};
    }
  return image;
}
double Mse(const Image& image, const Image& reference) {
  if (image.width != reference.width || image.height != reference.height || image.pixels.empty())
    throw std::invalid_argument("Image dimensions differ or are empty");
  double sum = 0;
  for (size_t i = 0; i < image.pixels.size(); ++i)
    for (int c = 0; c < 3; ++c) {
      double delta = double(image.pixels[i][c]) - reference.pixels[i][c];
      sum += delta * delta;
    }
  return sum / (3 * image.pixels.size());
}
}  // namespace lumen
