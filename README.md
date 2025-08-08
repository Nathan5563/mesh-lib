# mesh-lib

## Requirements

- Linux environment
- GCC
- libfmt-dev
- Make

## Installation

1. Clone the repository:

   ```bash
   git clone <repository-url>
   cd mesh-lib
   ```

2. Build the project:
   ```bash
   make
   ```

## Usage

Run the harness program with the path to your obj file:

```bash
./bin/harness <path-to-obj-file>
```

## Mesh Data Structure

The mesh is represented as a struct containing the following:

- `std::vector<Vertex> vertices`
  - `Vertex` is a struct composed of three `float`s to represent x, y, and z coordinates.
- `std::vector<Texture> textures`
  - `Texture` is a struct composed of two `float`s to represent u (horizontal) and v (vertical) axes.
- `std::vector<Normal> normals`
  - `Normal` is a struct composed of three `float`s to represent x, y, and z coordinates.
- `std::vector<Face> faces`
  - `Face` is a struct composed of nine `size_t`s to index into the above vectors. This means it is currently limited to parsing triangles and will ignore additional indices.

Both `vertices` and `faces` are 0-indexed, so the obj input and output correctly handles 1-based indexing in the obj file by adding or subtracting 1 when appropriate. All indices in this representation are positive, so negative indices in obj input are also resolved.

`.mtl` parsing is not yet supported.

## Performance Analysis

Analysis and comparisons were done using the Linux `/usr/bin/time` tool. The test harness loads the obj file into whatever data structure is used to represent the mesh, then returns after freeing memory. Optimizations were chosen based on performance analysis from the following tools:

- `perf stat`
- `perf record` with `perf report` TUI
- `valgrind --tool=callgrind` with `kcachegrind` GUI

Single-threaded optimizations include:

- Using `mmap` with `MAP_POPULATE` to load the file into memory at once
- Using the `fast_float` library instead of `stoi`/`stof` for faster string-to-float conversions
- Using pointers and `std::string_view` everywhere to avoid heap allocations with `std::string`

And for machines supporting parallelism, large files are broken into chunks and parsed in parallel using the same optimizations, then later merged in order once all threads have completed their jobs. Results have shown that the added overhead for managing threads is only really an issue for very small files (less than 1MB), and even then, it is on the order of milliseconds.

Output of `perf record` for a 2.5GB file on the single-threaded implementation:

```bash
# Total Lost Samples: 0
#
# Samples: 16K of event 'task-clock:uppp'
# Event count (approx.): 4210500000
#
# Overhead  Command  Shared Object         Symbol                              
# ........  .......  ....................  ....................................
#
    26.97%  harness  harness               [.] parseLine(Mesh&, std::basic_s...
    23.43%  harness  harness               [.] fast_float::from_chars_result...
    21.49%  harness  harness               [.] parseIndex(char const*, char ...
    15.27%  harness  harness               [.] importMeshFromObj(Mesh&, char...
    11.69%  harness  harness               [.] fast_float::from_chars_result...
     1.14%  harness  libc.so.6             [.] __memmove_avx_unaligned_erms
     0.01%  harness  ld-linux-x86-64.so.2  [.] do_lookup_x
```

Output of `perf report` for the same file on the multi-threaded implementation:

```bash
# Total Lost Samples: 0
#
# Samples: 32K of event 'task-clock:uppp'
# Event count (approx.): 8071750000
#
# Overhead  Command  Shared Object         Symbol                              
# ........  .......  ....................  ....................................
#
    32.77%  harness  harness               [.] parseLine(Mesh&, std::basic_s...
    23.27%  harness  harness               [.] fast_float::from_chars_result...
    15.58%  harness  harness               [.] std::thread::_State_impl<std:...
    13.54%  harness  harness               [.] parseIndex(char const*, char ...
     8.57%  harness  harness               [.] fast_float::from_chars_result...
     5.31%  harness  libc.so.6             [.] __memmove_avx_unaligned_erms
     0.93%  harness  harness               [.] importMeshFromObjParallel(Mes...
     0.01%  harness  libc.so.6             [.] cfree@GLIBC_2.2.5
```

This shows that the bottleneck is still the expensive string-to-float conversions in both implementations.

## Comparison with other parsers

These tests were run on a device with the following specifications:

- CPU: Intel Core Ultra 9 185H, 16 cores, 22 threads
- RAM: 32GB, 7467 MT/s
- SSD: 5527 MB/s for sequential reads, 2208 MB/s for sequential writes

Because testing like this is non-deterministic, the following results are approximations that try to reduce noise by "warming up" memory by doing a few parse rounds before starting the tests, averaging 20+ runs per test, and rejecting outliers via median absolute deviation. All parsers were tested on the following images:

- Stanford Bunny model ([source](https://casual-effects.com/data/), 10MB file, 72K vertices, 144K faces, triangulated)
- Chinese Dragon model ([source](https://casual-effects.com/data/), 72MB file, 439K vertices, 871K faces, triangulated)
- Rungholt Minecraft map ([source](https://casual-effects.com/data/), 270MB file, 2.5M vertices, 3.4M faces, not triangulated)
- Blender 3.0 splash screen ([source](https://download.blender.org/archive/gallery/blender-splash-screens/blender-3-0/), 2.5GB file, 14.4M vertices, 14.3M faces, not triangulated)

<img src="https://casual-effects.com/g3d/data10/research/model/bunny/icon.png" alt="Stanford Bunny graphics model" /> <img src="https://casual-effects.com/g3d/data10/research/model/dragon/icon.png" alt="Chinese Dragon graphics model" /> <img src="https://casual-effects.com/g3d/data10/research/model/rungholt/icon.png" alt="Rungholt Minecraft map" /> <img src="https://github.com/user-attachments/assets/2d5aaad9-80eb-4e7b-bf6d-ca91e7e2e68b" alt="Blender 3.0 splash screen" width="300px" />

And yielded the following data:

![Stanford Bunny Performance](data/time/graphs/bunny_performance.png)

![Chinese Dragon Performance](data/time/graphs/dragon_performance.png)

![Rungholt Performance](data/time/graphs/rungholt_performance.png)

![Blender Splash Performance](data/time/graphs/blendersplash_performance.png)

