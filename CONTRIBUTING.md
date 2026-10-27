# Contributing

Use the checked-in clang-format configuration. Keep sampling densities and throughput updates consistent between CPU and CUDA. Add a numerical or geometric regression test when changing transport, intersections, or image interpretation.

Run CTest before submitting a change. A CPU build does not validate CUDA kernels. Record GPU hardware and compiler versions when reporting backend agreement or timing. Use concise Conventional Commit messages.
