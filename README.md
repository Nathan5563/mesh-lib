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
- `std::vector<Face> faces`
    - `Face` is a struct composed of three `size_t`s to index into three vertices in the `vertices` vector.

Note that both `vertices` and `faces` are 0-indexed, so the obj input and output correctly handles 1-based indexing in the obj file by adding or subtracting 1 when appropriate.

### Performance Analysis
Initial analysis was done using the Linux `time` tool. The test harness loads the obj file into whatever data structure is used to represent the mesh, then returns after freeing memory.

**Because testing like this is non-deterministic, the following results are approximations that try to reduce noise by averaging multiple runs.**

Optimizations were chosen based on performance analysis from the following tools:
- `perf stat`
- `perf record` with `perf report` TUI
- `valgrind --tool=callgrind` with `kcachegrind` GUI

These optimizations include using `mmap` to load the file into memory and avoid reallocations, using the `fast_float` library instead of `stoi`, aligning the memory-mapped region for better codegen, and using `memchr` instead of a `while` loop for better SIMD evaluation.

### Comparison with tinyobjloader
Using the following obj file,

<img src="https://github.com/user-attachments/assets/ef1643e0-1289-443e-a059-c70b4c84c5a8" alt="Chinese dragon model in Blender" width="400px" />

Averaged over 8 runs,
| Metric | `mesh-lib`                    | `tinyobjloader`               |
| ------ | ----------------------------- | ----------------------------- |
| real   | **0.095s**                     | **0.438s**                     |
| user   | **0.077s**                     | **0.288s**                     |
| sys    | **0.028s**                     | **0.163s**                     |

`tinyobjloader` is slower here due to more page faults, according to `perf stat`. `mesh-lib` records **14,710** page faults, whereas `tinyobjloader` records **50,961**. `mesh-lib` also has less functionality, but I'm not sure yet how much, if at all, the extra functionality plays a part.

The difference becomes bigger with the following 2.5 GB obj file:

<img src="https://github.com/user-attachments/assets/2d5aaad9-80eb-4e7b-bf6d-ca91e7e2e68b" alt="Blender 3.0 splash screen" width="500px" />

Averaged over 8 runs,
| Metric | `mesh-lib`                    | `tinyobjloader`               |
| ------ | ----------------------------- | ----------------------------- |
| real   | **2.661s**                     | **19.312s**                     |
| user   | **1.900s**                     | **15.987s**                     |
| sys    | **0.618s**                     | **2.747s**                     |

Note that the values for `tinyobjloader` match the values measured in this [blog post](https://aras-p.info/blog/2022/05/14/comparing-obj-parse-libraries/).
