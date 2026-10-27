#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>

#include "lumen/render.h"

int main(int argc, char** argv) {
  if (!glfwInit()) {
    std::cerr << "GLFW initialization failed\n";
    return 1;
  }
  GLFWwindow* window = glfwCreateWindow(720, 720, "LUMEN", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  int status = 0;
  try {
    auto scene = lumen::MakeScene(argc > 1 ? argv[1] : "cornell");
    lumen::Settings s;
    s.width = 360;
    s.height = 360;
    s.samples = 1;
    lumen::Image film{s.width, s.height, std::vector<lumen::Vec3>(size_t(s.width) * s.height)};
    unsigned samples = 0;
    float exposure = 0;
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
      if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) exposure += .05f;
      if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) exposure -= .05f;
      if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        samples = 0;
        std::fill(film.pixels.begin(), film.pixels.end(), lumen::Vec3{});
      }
      s.sample_offset = samples;
      auto next = lumen::RenderCpu(scene, s).image;
      for (size_t i = 0; i < film.pixels.size(); ++i)
        film.pixels[i] = (film.pixels[i] * float(samples) + next.pixels[i]) / float(samples + 1);
      ++samples;
      auto bytes = lumen::DisplayPixels(film, exposure);
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);
      glViewport(0, 0, width, height);
      glClear(GL_COLOR_BUFFER_BIT);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glRasterPos2f(-1, 1);
      glPixelZoom(float(width) / s.width, -float(height) / s.height);
      glDrawPixels(s.width, s.height, GL_RGB, GL_UNSIGNED_BYTE, bytes.data());
      glfwSetWindowTitle(
          window,
          ("LUMEN | " + std::to_string(samples) + " spp | arrows: exposure | R: reset").c_str());
      glfwSwapBuffers(window);
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    status = 1;
  }
  glfwDestroyWindow(window);
  glfwTerminate();
  return status;
}
