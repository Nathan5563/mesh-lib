# mesh-lib

### Requirements

- Linux environment
- C++ compiler installed (`g++`)

### Installation

1. Clone the repository:

   ```bash
   git clone <repository-url>
   cd mesh-lib
   ```

2. Build the project:
   ```bash
   make
   ```

### Usage

Run the harness program with the path to your obj file:

```bash
./bin/harness <path-to-obj-file>
```

Optionally, uncomment line 20 in `tests/harness.cpp` to output the parsed obj file, then run:

```bash
./bin/harness <path-to-obj-file> > output.obj
```

### Mesh Data Structure

The mesh is represented as a struct containing the following:

- `std::vector<Vertex> vertices`
  - `Vertex` is a struct composed of three `float`s to represent x, y, and z coordinates.
- `std::vector<Texture> textures`
  - `Texture` is a struct composed of two `float`s to represent u (horizontal) and v (vertical) axes.
- `std::vector<Normal> normals`
  - `Normal` is a struct composed of three `float`s to represent x, y, and z coordinates.
- `std::vector<Face> faces`
  - `Face` is a struct composed of nine `size_t`s to index into the above vectors.

Both `vertices` and `faces` are 0-indexed, so the obj input and output correctly handles 1-based indexing in the obj file by adding or subtracting 1 when appropriate. All indices in this representation are positive, so negative indices in obj input are also resolved.

`.mtl` parsing is not yet supported.

### Performance Analysis

Initial analysis was done using the Linux `time` tool. The test harness loads the obj file into whatever data structure is used to represent the mesh, then returns after freeing memory. Optimizations were chosen based on performance analysis from the following tools:

- `perf stat`
- `perf record` with `perf report` TUI
- `valgrind --tool=callgrind` with `kcachegrind` GUI

Single-threaded optimizations include using `mmap` to load the file into memory and avoid reallocations, using the `fast_float` library instead of `stoi`/`stof`, and aligning the memory-mapped region to 64B for better codegen.

I implemented a two-pass parser in the `two-pass` branch, the first pass being used to count the number of vertices and faces to avoid multiple reallocations. The performance ended up being similar to the single-threaded implementation, so I have not merged it into main. The branch has fallen behind in development (it doesn't support textures and normals, for example).

#### Comparison with tinyobjloader

Because testing like this is non-deterministic, the following results are approximations that try to reduce noise by averaging multiple runs. 

Using the following ~70M obj file (source [here](https://download.blender.org/archive/gallery/blender-splash-screens/blender-3-0/)),

<img src="https://github.com/user-attachments/assets/ef1643e0-1289-443e-a059-c70b4c84c5a8" alt="Chinese dragon model in Blender" width="400px" />

Averaged over 8 runs,
| Metric | `mesh-lib` | `mesh-lib-parallel` | `tinyobjloader` |
| ------ | ---------- | --------------------|---------------- |
| real   | **0.182s** | **0.111s**          | **0.438s**      |
| user   | **0.129s** | **0.252s**          | **0.288s**      |
| sys    | **0.041s** | **0.126s**          | **0.163s**      |

The difference becomes bigger with the following ~2.5G obj file (source [here](https://casual-effects.com/data/)):

<img src="https://github.com/user-attachments/assets/2d5aaad9-80eb-4e7b-bf6d-ca91e7e2e68b" alt="Blender 3.0 splash screen" width="500px" />

Averaged over 8 runs,
| Metric | `mesh-lib` | `mesh-lib-parallel` | `tinyobjloader` |
| ------ | ---------- | --------------------|---------------- |
| real   | **4.707s** | **2.044s**          | **19.312s**     |
| user   | **3.854s** | **8.395s**          | **15.987s**     |
| sys    | **0.796s** | **2.875s**          | **2.747s**      |

The values for `tinyobjloader` match the values measured in this [blog post](https://aras-p.info/blog/2022/05/14/comparing-obj-parse-libraries/).

#### Further Optimization

Output of `perf report` for the Blender splash screen on the single-threaded implementation:

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

Output of `perf report` for the Blender splash screen on the multi-threaded implementation:

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

The added overhead for managing threads is only an issue for very small files. Even the smallest file tested (~35M) showed minor benefits with parallelism.

===========================================================================

// TODO: Clean up and format nicely into README

FOR blendersplash.obj,
- INITIAL WRITE TIME: 24s (using std::cout)
- OPTIMIZED WRITE TIME: 11s (using fmt, reserved buffer, write syscall)

Writes are still very slow compared to reads:
- Single-threaded read: 4-5s
- Single-threaded write: 10-12s
- Multithreaded read: 2-3s
- Multithreaded write: TODO
