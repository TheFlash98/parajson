# Parajson

---

## Overview

**Parajson** is a **JSON parser** specifically engineered and optimized for **parallel processing**. It leverages multi-threading to significantly speed up the parsing of large JSON files.

---

## Build and Installation

### Prerequisites

* A C++ compiler supporting **C++17** or later (e.g., GCC, Clang).
* **CMake** (version 3.26 or higher).
* **Make** or another compatible build tool.

### Steps

1.  **Create a build directory:**
    ```bash
    mkdir build && cd build
    ```

2.  **Generate the build system files:**
    ```bash
    cmake ..
    ```

3.  **Compile the project:**
    ```bash
    make
    ```
    This will compile the `parajson` executable and place it in the `build/` directory.

---

## Usage

The `parajson` executable is run from the command line and uses environment variables for parallelism settings and flags for configuration.

```bash
PARLAY_NUM_THREADS=32 ./parajson -f /path/to/your/file.json
```

### Command Line Arguments

| Argument | Description | Default |
| :--- | :--- | :--- |
| **`-f`** `file` | **Required.** Path to the JSON file to be parsed. | *None* |
| **`PARLAY_NUM_THREADS`** | **Environment Variable.** Sets the number of threads for parallel execution. | *Defaults to highest value* |
| **`-w`** `warmups` | Number of warmup runs to execute before iterations begin. | `1` |
| **`-r`** `repeats` | Number of timed iterations to repeat for collecting average performance statistics. | `1` |
| **`-c`** `chunk_size` | Maximum size of each chunk for parallel processing in **Stage 2** of the parsing pipeline. | *1024* |

### Example for Benchmarking

To run the parser with 16 threads, 5 warmup runs, and 50 repeated timed runs on a specific file:

```bash
PARLAY_NUM_THREADS=16 ./parajson -r 50 -w 5 -f data/large_dataset.json
```