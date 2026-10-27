#ifndef LUMEN_MATH_H_
#define LUMEN_MATH_H_

#include <cmath>
#include <cstdint>

#ifdef __CUDACC__
#define LUMEN_HD __host__ __device__
#else
#define LUMEN_HD
#endif

namespace lumen {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 0.0001f;
constexpr float kInfinity = 1.0e30f;

struct Vec3 {
  float x = 0, y = 0, z = 0;
  LUMEN_HD float operator[](int i) const { return i == 0 ? x : i == 1 ? y : z; }
};
LUMEN_HD inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
LUMEN_HD inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
LUMEN_HD inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
LUMEN_HD inline Vec3 operator*(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
LUMEN_HD inline Vec3 operator*(Vec3 a, float b) { return {a.x * b, a.y * b, a.z * b}; }
LUMEN_HD inline Vec3 operator*(float b, Vec3 a) { return a * b; }
LUMEN_HD inline Vec3 operator/(Vec3 a, float b) { return a * (1.0f / b); }
LUMEN_HD inline Vec3& operator+=(Vec3& a, Vec3 b) {
  a = a + b;
  return a;
}
LUMEN_HD inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
LUMEN_HD inline Vec3 Cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
LUMEN_HD inline float Length(Vec3 v) { return sqrtf(Dot(v, v)); }
LUMEN_HD inline Vec3 Normalize(Vec3 v) { return v / Length(v); }
LUMEN_HD inline float MaxComponent(Vec3 v) { return fmaxf(v.x, fmaxf(v.y, v.z)); }
LUMEN_HD inline Vec3 Min(Vec3 a, Vec3 b) {
  return {fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)};
}
LUMEN_HD inline Vec3 Max(Vec3 a, Vec3 b) {
  return {fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)};
}
LUMEN_HD inline float Clamp(float v, float lo, float hi) { return fminf(hi, fmaxf(lo, v)); }
LUMEN_HD inline Vec3 Reflect(Vec3 v, Vec3 n) { return v - n * (2 * Dot(v, n)); }
LUMEN_HD inline float PowerWeight(float a, float b) {
  float scale = fmaxf(a, b);
  if (scale <= 0) return 0;
  a /= scale;
  b /= scale;
  return a * a / (a * a + b * b);
}

struct Rng {
  uint32_t state;
  LUMEN_HD explicit Rng(uint32_t seed = 1) : state(seed ? seed : 1) {}
  LUMEN_HD float Next() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return float(state >> 8) * (1.0f / 16777216.0f);
  }
};
LUMEN_HD inline uint32_t Seed(uint32_t pixel, uint32_t sample, uint32_t seed) {
  uint32_t value = pixel * 0x9e3779b9u ^ sample * 0x85ebca6bu ^ seed;
  value ^= value >> 16;
  value *= 0x7feb352du;
  value ^= value >> 15;
  value *= 0x846ca68bu;
  return (value ^ (value >> 16)) | 1u;
}
LUMEN_HD inline Vec3 CosineDirection(Vec3 n, Rng& rng) {
  float u = rng.Next(), angle = 2 * kPi * rng.Next();
  float radius = sqrtf(u);
  Vec3 tangent = Normalize(Cross(fabsf(n.x) > .9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0}, n));
  Vec3 bitangent = Cross(n, tangent);
  return Normalize(tangent * (radius * cosf(angle)) + bitangent * (radius * sinf(angle)) +
                   n * sqrtf(fmaxf(0, 1 - u)));
}
struct Ray {
  Vec3 origin, direction;
};
struct Bounds {
  Vec3 lo{kInfinity, kInfinity, kInfinity}, hi{-kInfinity, -kInfinity, -kInfinity};
  LUMEN_HD bool Intersects(Ray ray, float maximum) const {
    float minimum = kEpsilon;
    for (int axis = 0; axis < 3; ++axis) {
      float d = ray.direction[axis], o = ray.origin[axis];
      if (fabsf(d) < 1e-20f) {
        if (o < lo[axis] || o > hi[axis]) return false;
      } else {
        float a = (lo[axis] - o) / d, b = (hi[axis] - o) / d;
        minimum = fmaxf(minimum, fminf(a, b));
        maximum = fminf(maximum, fmaxf(a, b));
        if (maximum < minimum) return false;
      }
    }
    return true;
  }
};
}  // namespace lumen
#endif
