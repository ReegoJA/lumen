#include <cuda_runtime.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

#include "lumen/integrator.h"
#include "lumen/render.h"

namespace lumen {
namespace {
void Check(cudaError_t status) {
  if (status != cudaSuccess) throw std::runtime_error(cudaGetErrorString(status));
}
template <class T>
class DeviceBuffer {
 public:
  explicit DeviceBuffer(size_t count) : count_(count) {
    if (count_) Check(cudaMalloc(reinterpret_cast<void**>(&data_), count_ * sizeof(T)));
  }
  ~DeviceBuffer() {
    if (data_) cudaFree(data_);
  }
  DeviceBuffer(const DeviceBuffer&) = delete;
  DeviceBuffer& operator=(const DeviceBuffer&) = delete;
  T* Get() const { return data_; }
  void Upload(const T* source) {
    if (count_) Check(cudaMemcpy(data_, source, count_ * sizeof(T), cudaMemcpyHostToDevice));
  }

 private:
  T* data_ = nullptr;
  size_t count_;
};
__global__ void Generate(Path* paths, int* queue, Camera camera, Settings settings, int sample) {
  int pixel = blockIdx.x * blockDim.x + threadIdx.x;
  if (pixel >= settings.width * settings.height) return;
  Path path;
  path.rng = Rng(Seed(pixel, sample + settings.sample_offset, settings.seed));
  path.ray = camera.Generate(pixel % settings.width, pixel / settings.width, settings.width,
                             settings.height, path.rng);
  paths[pixel] = path;
  queue[pixel] = pixel;
}
__global__ void Trace(SceneView scene, Path* paths, const int* queue, int count, Hit* hits,
                      bool brute) {
  int index = blockIdx.x * blockDim.x + threadIdx.x;
  if (index >= count) return;
  int pixel = queue[index];
  hits[pixel] = Intersect(scene, paths[pixel].ray, kInfinity, brute);
}
__global__ void Scatter(SceneView scene, Path* paths, const int* queue, int count, const Hit* hits,
                        Shadow* shadows, int* next, int* next_count, int depth, Settings settings) {
  int index = blockIdx.x * blockDim.x + threadIdx.x;
  if (index >= count) return;
  int pixel = queue[index];
  shadows[pixel] = Shade(scene, hits[pixel], paths[pixel], depth, settings.max_depth, settings.mis);
  if (paths[pixel].alive) next[atomicAdd(next_count, 1)] = pixel;
}
__global__ void TraceShadows(SceneView scene, Path* paths, const int* queue, int count,
                             const Shadow* shadows, bool brute) {
  int index = blockIdx.x * blockDim.x + threadIdx.x;
  if (index >= count) return;
  int pixel = queue[index];
  Shadow shadow = shadows[pixel];
  if (shadow.active && Intersect(scene, shadow.ray, shadow.maximum, brute).primitive < 0)
    paths[pixel].radiance += shadow.contribution;
}
__global__ void Accumulate(const Path* paths, Vec3* film, int count) {
  int pixel = blockIdx.x * blockDim.x + threadIdx.x;
  if (pixel < count) film[pixel] += paths[pixel].radiance;
}
}  // namespace

RenderResult RenderCuda(const Scene& scene, const Settings& settings) {
  Validate(settings);
  Check(cudaFree(nullptr));
  auto start = std::chrono::steady_clock::now();
  int count = settings.width * settings.height;
  DeviceBuffer<Primitive> primitives(scene.primitives.size());
  primitives.Upload(scene.primitives.data());
  DeviceBuffer<Material> materials(scene.materials.size());
  materials.Upload(scene.materials.data());
  DeviceBuffer<Node> nodes(scene.nodes.size());
  nodes.Upload(scene.nodes.data());
  DeviceBuffer<int> indices(scene.indices.size());
  indices.Upload(scene.indices.data());
  DeviceBuffer<int> lights(scene.lights.size());
  lights.Upload(scene.lights.data());
  DeviceBuffer<Path> paths(count);
  DeviceBuffer<Hit> hits(count);
  DeviceBuffer<Shadow> shadows(count);
  DeviceBuffer<int> first(count), second(count), next_count(1);
  DeviceBuffer<Vec3> film(count);
  Check(cudaMemset(film.Get(), 0, count * sizeof(Vec3)));
  SceneView view{primitives.Get(),
                 materials.Get(),
                 nodes.Get(),
                 indices.Get(),
                 lights.Get(),
                 int(scene.primitives.size()),
                 int(scene.nodes.size()),
                 int(scene.lights.size())};
  constexpr int block = 128;
  for (int sample = 0; sample < settings.samples; ++sample) {
    int* current = first.Get();
    int* next = second.Get();
    Generate<<<(count + block - 1) / block, block>>>(paths.Get(), current, scene.camera, settings,
                                                     sample);
    Check(cudaGetLastError());
    int active = count;
    for (int depth = 0; depth < settings.max_depth && active; ++depth) {
      Check(cudaMemset(next_count.Get(), 0, sizeof(int)));
      int blocks = (active + block - 1) / block;
      Trace<<<blocks, block>>>(view, paths.Get(), current, active, hits.Get(), settings.brute);
      Check(cudaGetLastError());
      Scatter<<<blocks, block>>>(view, paths.Get(), current, active, hits.Get(), shadows.Get(),
                                 next, next_count.Get(), depth, settings);
      Check(cudaGetLastError());
      TraceShadows<<<blocks, block>>>(view, paths.Get(), current, active, shadows.Get(),
                                      settings.brute);
      Check(cudaGetLastError());
      Check(cudaMemcpy(&active, next_count.Get(), sizeof(int), cudaMemcpyDeviceToHost));
      std::swap(current, next);
    }
    Accumulate<<<(count + block - 1) / block, block>>>(paths.Get(), film.Get(), count);
    Check(cudaGetLastError());
  }
  RenderResult result;
  result.image = {settings.width, settings.height, std::vector<Vec3>(count)};
  Check(cudaMemcpy(result.image.pixels.data(), film.Get(), count * sizeof(Vec3),
                   cudaMemcpyDeviceToHost));
  for (auto& pixel : result.image.pixels) pixel = pixel / float(settings.samples);
  result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  result.working_bytes =
      size_t(count) *
          (sizeof(Path) + sizeof(Hit) + sizeof(Shadow) + 2 * sizeof(int) + sizeof(Vec3)) +
      scene.primitives.size() * sizeof(Primitive) + scene.materials.size() * sizeof(Material) +
      scene.nodes.size() * sizeof(Node) +
      (scene.indices.size() + scene.lights.size() + 1) * sizeof(int);
  return result;
}
}  // namespace lumen
