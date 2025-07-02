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
Run the harness program with the path to your `.obj` file:
```bash
./bin/harness <path-to-obj-file>
```

Optionally, redirect the output into a file:
```bash
./bin/harness <path-to-obj-file> > output.obj
```
