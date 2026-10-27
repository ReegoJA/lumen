# Rendering checks

Recorded October 26, 2026. These are initial correctness and convergence checks across three procedural scenes, not a comprehensive performance study.

## Convergence

Mean linear RGB squared error over seeds 1, 2, and 3 against independent 1,024-sample references at 96 by 96 pixels. The references use seed 7001 and retain sampling error.

| Scene | 8 samples | 32 samples | 128 samples |
| --- | --- | --- | --- |
| Cornell | 0.01358 | 0.00430 | 0.00100 |
| Glass | 0.07270 | 0.02067 | 0.00557 |
| Spheres | 0.05017 | 0.01287 | 0.00340 |

Error decreases as sampling increases in each scene. Raw [measurements](benchmarks/measurements.csv) and [environment](benchmarks/environment.json) accompany the results. A matched BVH and brute-force spheres render produced equal error, with wall times of 0.152 and 1.062 seconds respectively. This single timing pair is illustrative.

## CPU and CUDA

NVIDIA A100 80 GB PCIe, CUDA 12.6, GCC 12.3, four CPU threads, 160 by 160 pixels, 64 samples per pixel, depth 12, seed 1. Each row is one CPU render and one CUDA render on the same node.

| Scene | CPU seconds | CUDA seconds | CPU/CUDA linear RGB MSE |
| --- | --- | --- | --- |
| Cornell | 1.578 | 0.084 | 0.00000182 |
| Glass | 1.234 | 0.068 | 0.00001010 |
| Spheres | 2.736 | 0.233 | 0.00029651 |

Corresponding CSV files are under [benchmarks](benchmarks). Different floating-point paths can produce different samples despite matched random-stream identities. These errors describe observed agreement, not a universal tolerance. The smaller 24 by 24, 32-sample correctness fixture passed its normalized-error threshold.

The Georgia Tech PACE Phoenix cluster was used to run the GPU experiments.

Times include renderer allocations and synchronization, excluding initial CUDA context setup, scene construction, BVH construction, and file output. Single-run timings cannot establish stable speedups. Buffer estimates in the CSV files are not peak memory measurements.

## Coverage

The CPU suite checks primitive intersections, 20,000 BVH comparisons, a cosine-sampling moment, Fresnel boundary cases, thread-count determinism, finite radiance, and floating-point image round trips. An independent-seed MIS versus BSDF-only energy comparison checks for large estimator discrepancies. This does not prove unbiasedness or physical correctness for arbitrary scenes.

The OpenGL viewer displays CPU renders. Interactive viewer behavior requires a graphics-capable desktop and is separate from the headless CUDA validation.
