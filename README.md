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

Note that both `vertices` and `faces` are 0-indexed, so the obj input and output correctly handles 1-based indexing in the obj file.

### Performance Analysis
Initial analysis was done using the Linux `time` tool and the following obj file:

<img src="https://github.com/user-attachments/assets/ef1643e0-1289-443e-a059-c70b4c84c5a8" alt="Chinese dragon model in Blender" width="500px" />

The test harness loads the obj file into whatever data structure is used to represent the mesh, then returns after freeing memory.

**Because testing like this is non-deterministic, the following results are approximations that try to reduce noise by averaging multiple runs.**

Optimizations were chosen based on performance analysis from the following tools:
- `perf stat`
- `perf record` with `perf report` TUI
- `valgrind --tool=callgrind` with `kcachegrind` GUI

#### Optimization 1:
`std::from_chars` is much faster than `stoi`/`stof`. Averaged over 8 runs,
| Metric | `std::stoi` and `std::stof` | `std::from_chars` |
| ------ | --------------------------- | ----------------- |
| real   | **0.416**                   | **0.338**         |
| user   | **0.366**                   | **0.283**         |
| sys    | **0.055**                   | **0.051**         |

#### Optimization 2:
`mmap` with `std::string_view` is faster than `ifstream` and `std::getline` (especially for large files). Averaged over 8 runs,
| Metric | `ifstream` and `std::getline` | `mmap` and `std::string_view` |
| ------ | ----------------------------- | ----------------------------- |
| real   | **0.338**                     | **0.224**                     |
| user   | **0.283**                     | **0.202**                     |
| sys    | **0.051**                     | **0.026**                     |

### Comparison with tinyobjloader
Using the final version of the library thus far (after optimizations 1 and 2), averaged over 8 runs,
| Metric | `mesh-lib`                    | `tinyobjloader`               |
| ------ | ----------------------------- | ----------------------------- |
| real   | **0.224**                     | **0.309**                     |
| user   | **0.202**                     | **0.213**                     |
| sys    | **0.026**                     | **0.099**                     |

`tinyobjloader` is slightly slower here due to more page faults, according to `perf stat`. `mesh-lib` records **14,711** page faults, whereas `tinyobjloader` records **50,961**. `mesh-lib` also has less functionality, but I'm not sure yet how much, if at all, the extra functionality plays a part.
