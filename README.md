# Ray Tracing Further

## Introduction

This repo serves some fun, further personal learnings following the completion of [RTOW](https://raytracing.github.io/books/RayTracingInOneWeekend.html). There are a few features I will implement:
1. Multithreading to speed up path tracing by parallelizing over pixels (this problem is embarrassingly parallel)
   1. Extension to CUDA if I have time
2. Enable simple procedural animation based on randomly generated scenes. 
   1. Add simple UV map to be able to visualize rotation on spheres
   2. Define each sphere with some initial velocity
   3. Add rotation mechanics
   4. Add collision detection using the inherent properties of spheres
   5. Render each frame and compile into a single animation
3. Anything else I can think of

## Effects of Parallelization

These benchmarks are performed on 7800x3d.