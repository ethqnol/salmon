# Salmon

Salmon is a statically typed, compiled toy language targeting LLVM IR.


## Prerequisites

To build the Salmon compiler, ensure you have the following installed:

- **C++ Compiler**: GCC (`g++`) or Clang (`clang++`) with C++20 support.
- **LLVM**: LLVM development libraries and headers (providing `llvm-config`).
- **Build Tools**: `make`.
- **Linker Driver**: `clang` (used by Salmon to link object files with `libc` and `libm`).

### Installing Dependencies

#### Ubuntu / Debian
```bash
sudo apt-get update
sudo apt-get install -y build-essential clang llvm-dev
```

#### Fedora / RHEL
```bash
sudo dnf install -y gcc-c++ clang llvm-devel make
```

#### Arch Linux
```bash
sudo pacman -S base-devel clang llvm
```

#### macOS (Homebrew)
```bash
brew install llvm make
```

#### Windows
No idea ask microsoft


---

## Building

Clone the repository and build the `salmon` compiler binary using `make`:

```bash
# Build the compiler
make -j$(nproc)
```

To remove build artifacts:

```bash
make clean
```

---

## Usage

```bash
salmon [options] <source_file>
```

### Compiler Options

| Option | Description |
| :--- | :--- |
| `-o <file>` | Specify output binary or file name (default: `./<stem>`) |
| `-run`, `--run`, `-r` | Compile and execute the binary immediately |
| `-S` | Compile to target assembly (`.s`) |
| `--emit-llvm`, `--ir` | Emit LLVM IR (`.ll`) |
| `-p`, `--pretty` | Pretty-print formatted source code from AST |
| `--tree` | Print visual AST hierarchy tree |
| `--asdl` | Print Zephyr ASDL AST representation |
| `-t`, `--tokens` | Print lexer tokens |
| `-h`, `--help` | Show help message |

---

## Examples

Run any of the provided example programs directly with `-r`:

```bash
# Sieve of Eratosthenes (Prime Number Generator)
./salmon -r examples/sieve.salmon

# Linked List (Heap allocation & free)
./salmon -r examples/linked_list.salmon

# Dynamic lists and slices
./salmon -r examples/list_array.salmon

# First-class string operations
./salmon -r tests/test_strings.sal
```

### Compiling to a Standalone Executable

```bash
# Compile to a native binary 'my_program'
./salmon examples/sieve.salmon -o my_program

# Run the compiled binary
./my_program
```
