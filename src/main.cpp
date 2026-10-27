#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "lumen/render.h"

int main(int argc, char** argv) {
  try {
    lumen::Settings settings;
    float light_x = 0;
    std::string scene_name = "cornell", backend = "cpu", output = "render", reference;
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--help") {
        std::cout
            << "lumen [--scene cornell|glass|spheres] [--backend cpu|cuda]\n"
               "      [--width N] [--height N] [--spp N] [--depth N]\n"
               "      [--threads N] [--seed N] [--offset N] [--output PREFIX]\n"
               "      [--integrator mis|bsdf] [--traversal bvh|brute] [--reference FILE.pfm]\n"
               "      [--light-x OFFSET] (between -0.6 and 0.6)\n";
        return 0;
      }
      if (i + 1 >= argc) throw std::invalid_argument("Missing value for " + arg);
      std::string value = argv[++i];
      auto number = [&]() {
        size_t end = 0;
        int result = std::stoi(value, &end);
        if (end != value.size() || result < 0)
          throw std::invalid_argument("Invalid integer: " + value);
        return result;
      };
      if (arg == "--light-x") {
        size_t end = 0;
        light_x = std::stof(value, &end);
        if (end != value.size() || !std::isfinite(light_x) || std::abs(light_x) > .6f)
          throw std::invalid_argument("Light offset must be between -0.6 and 0.6");
      } else if (arg == "--scene")
        scene_name = value;
      else if (arg == "--backend")
        backend = value;
      else if (arg == "--width")
        settings.width = number();
      else if (arg == "--height")
        settings.height = number();
      else if (arg == "--spp")
        settings.samples = number();
      else if (arg == "--depth")
        settings.max_depth = number();
      else if (arg == "--threads")
        settings.threads = number();
      else if (arg == "--seed")
        settings.seed = number();
      else if (arg == "--offset")
        settings.sample_offset = number();
      else if (arg == "--output")
        output = value;
      else if (arg == "--reference")
        reference = value;
      else if (arg == "--integrator" && (value == "mis" || value == "bsdf"))
        settings.mis = value == "mis";
      else if (arg == "--traversal" && (value == "bvh" || value == "brute"))
        settings.brute = value == "brute";
      else
        throw std::invalid_argument("Unknown option or value: " + arg + " " + value);
    }
    lumen::Validate(settings);
    lumen::Scene scene = lumen::MakeScene(scene_name);
    for (auto& primitive : scene.primitives) {
      if (lumen::MaxComponent(scene.materials[primitive.material].emission) <= 0) continue;
      primitive.a.x += light_x;
      primitive.b.x += light_x;
      primitive.c.x += light_x;
    }
    if (light_x != 0) scene.Build();
    lumen::RenderResult result;
    if (backend == "cpu") result = lumen::RenderCpu(scene, settings);
#ifdef LUMEN_HAS_CUDA
    else if (backend == "cuda")
      result = lumen::RenderCuda(scene, settings);
#endif
    else
      throw std::invalid_argument("Backend is unavailable in this build: " + backend);
    auto parent = std::filesystem::path(output).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    lumen::WritePpm(result.image, output + ".ppm");
    lumen::WritePfm(result.image, output + ".pfm");
    double mse = -1;
    if (!reference.empty()) mse = lumen::Mse(result.image, lumen::ReadPfm(reference));
    std::ofstream metrics(output + ".csv");
    if (!metrics) throw std::runtime_error("Cannot open metrics output");
    metrics << "scene,backend,width,height,spp,depth,seed,offset,mis,bvh,seconds,samples_per_"
               "second,working_bytes,mse,light_x\n";
    metrics << std::setprecision(12) << scene_name << ',' << backend << ',' << settings.width << ','
            << settings.height << ',' << settings.samples << ',' << settings.max_depth << ','
            << settings.seed << ',' << settings.sample_offset << ',' << settings.mis << ','
            << !settings.brute << ',' << result.seconds << ','
            << double(settings.width) * settings.height * settings.samples / result.seconds << ','
            << result.working_bytes << ',';
    if (mse >= 0) metrics << mse;
    metrics << ',' << light_x << '\n';
    std::cout << "Rendered " << scene_name << " at " << settings.samples << " spp in "
              << result.seconds << " seconds. Output: " << output << "\n";
    if (mse >= 0) std::cout << "Linear RGB MSE: " << mse << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "lumen: " << error.what() << '\n';
    return 1;
  }
}
