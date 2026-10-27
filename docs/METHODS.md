# Methods and limits

The CPU and CUDA paths share intersection, sampling, and shading functions. Sample identity depends on pixel, sample index, and seed, not worker order. CUDA queue compaction can change execution order without changing random-stream identity. Floating-point differences can still cause divergent paths, so cross-backend comparisons use image error rather than demanding identical bytes.

Diffuse surfaces use a Lambertian BSDF and cosine-weighted directions. One emissive primitive is sampled uniformly per diffuse vertex, followed by uniform area sampling. The area density is converted to a solid-angle density using squared distance and light cosine. Both light and BSDF samples receive complementary power-heuristic weights. Delta reflection and transmission bypass that mixture. Dielectric transmission applies the radiance transport factor from the index-of-refraction ratio. Emission is one-sided. Shadow rays treat glass as opaque, so direct paths through specular interfaces are found by BSDF paths rather than straight shadow connections.

The tree splits the longest centroid axis at its median with four primitives per leaf. This is a baseline BVH, not a surface-area-heuristic builder. Its validated depth is below the fixed traversal-stack capacity. Brute-force traversal is retained as a correctness reference. Ray origins use a fixed small offset appropriate to these unit-scale scenes, not a scale-independent robust intersection scheme.

Paths terminate at a finite vertex limit or by throughput-based Russian roulette after four scattering vertices. The finite limit truncates transport. Default depth is 12. Scenes use linear RGB and ideal diffuse, mirror, or glass materials. There are no participating media, rough microfacets, textures, nested dielectric volumes, spectral transport, denoising, or hardware ray-tracing acceleration.

CPU work is distributed by image row. CUDA uses active-pixel queues, separate intersection/shading/shadow kernels, atomic next-queue allocation, and one path per pixel for each sample. The host reads the active count after each depth. This straightforward implementation exposes synchronization overhead and is not claimed to be an optimized production wavefront renderer.

## Measurement

Compare errors in linear RGB before display mapping. The reference is an independent finite-sample estimate, not ground truth. Three seeds characterize an initial convergence check, not a comprehensive statistical study. Compare repeated timings on the same hardware and configuration before drawing speedup conclusions. Cross-machine CPU/GPU timings include different hardware and should be reported as such.

The renderer clock includes buffer allocation, worker startup, and CUDA transfer/kernel synchronization. It excludes scene construction, BVH build, output encoding, and the initial CUDA context setup. `working_bytes` estimates explicitly owned scene, image, and backend buffers, excluding allocator overhead, thread stacks, driver/runtime allocations, and process memory. It is not peak RAM or VRAM usage.

The GPU test is opt-in because CPU-only CI cannot validate execution on NVIDIA hardware. GPU completion and performance are reported only after a recorded device run. The OpenGL viewer is a display frontend for CPU rendering, not a claim of OpenGL compute integration.
