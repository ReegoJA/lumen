# References

- Matt Pharr, Wenzel Jakob, and Greg Humphreys. [Physically Based Rendering, fourth edition](https://pbr-book.org/4ed/contents). Light transport, BSDF sampling, multiple importance sampling, and wavefront organization. LUMEN implements a smaller renderer independently and does not copy the pbrt source.
- Samuli Laine, Tero Karras, and Timo Aila. [Megakernels Considered Harmful: Wavefront Path Tracing on GPUs](https://research.nvidia.com/sites/default/files/pubs/2013-07_Megakernels-Considered-Harmful/laine2013hpg_paper.pdf), 2013. Queue-based GPU execution architecture.
- [NVIDIA CUDA Programming Guide](https://docs.nvidia.com/cuda/cuda-programming-guide/index.html). Kernel execution, device memory, and synchronization.
- [GLFW documentation](https://www.glfw.org/docs/). Window and OpenGL context management.
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) and [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines), selected from [Awesome Guidelines](https://github.com/Kristories/awesome-guidelines). Formatting, ownership, and interface conventions. LUMEN uses exceptions at host-side CLI and allocation boundaries.

External dependencies retain their own licenses. Method references are attribution, not endorsements by their authors.
