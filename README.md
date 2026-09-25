# Getting started

Testlang is a stack-based, array-oriented programming language that compiles to native executables via LLVM. This guide will take you from installation to running your first program.

---

## System requirements

Before building Testlang, make sure your machine has the following installed.

| Requirement | Version | Notes |
|---|---|---|
| Operating system | Any Linux distribution | Debian / Ubuntu recommended |
| Python | 3.10 or later | Used by the lexer frontend |
| Clang++ | Any modern version | Must support C++17 |
| LLVM | Any modern version | `llvm-config` must be on your PATH |

> **Why Linux?** Testlang's build system is a Unix shell script and the compiler links against platform-specific LLVM libraries. While other Unix-like systems may work, a Debian-based Linux installation gives you the highest chance of a smooth build.

To check whether LLVM is available, run:

```bash
llvm-config --version
```

If the command is not found, install the LLVM development libraries:

```bash
# Debian / Ubuntu
sudo apt install llvm-dev clang python3
```

---

## Building from source

Clone or download the Testlang source, then run the two build scripts from the project root.

```bash
# Build the compiler and runtime
bash build.sh

# Build the bundled libraries (linalg, io, etc.)
bash build_libs.sh
```

`build.sh` checks for `llvm-config` and `python3` before compiling, and will print a clear error message if either is missing. A successful build produces:

- `./testlang` - the compiler executable
- `stack_runtime.bc` - the runtime library bitcode, linked into every compiled program

---

## Your first program

Testlang programs are plain text files with the `.stack` extension. Lines execute left to right, but within a line operations are applied **right to left** - the rightmost token runs first and pushes its result onto the stack for the next token to consume.

Create a file called `hello.stack`:

```
// Push 5 onto the stack, duplicate it, multiply the top two values, then print
; * . 5
```

Breaking this down right to left:

| Token | Action | Stack after |
|---|---|---|
| `5` | Push the number 5 | `[5]` |
| `.` | Duplicate the top | `[5, 5]` |
| `*` | Multiply the top two values | `[25]` |
| `;` | Pop and print | `[]` → prints `25.0` |

### Compile and run

```bash
./testlang hello.stack hello --exe
./hello
```

You should see:

```
25.0
```

The `--exe` flag tells the compiler to produce a standalone native executable. No interpreter or runtime needs to be present to run `./hello`.

---

## Variables and functions

Use `=>` to bind a name to an expression. If the expression always produces a fixed value, the name acts as a variable. If it consumes values from the stack, it acts as a function.

```
// Variable: pushes 3.14 whenever 'pi_approx' appears
pi_approx => 3.14

// Function: takes the top of the stack and returns its square
square => * . 

; square 9
```

Save this as `square.stack` and compile:

```bash
./testlang square.stack square --exe
./square
```

Output:

```
81.0
```

---

## Working with arrays

Arrays are defined with square brackets. All arithmetic operations are rank-polymorphic - they apply element-wise across arrays automatically.

```
// Add a scalar to every element of an array
; + [1 2 3 4 5] 10
```

```
// Multiply two arrays element-wise
; * [2 4 6] [3 3 3]
```

```
// Reshape a flat array into a 2×3 matrix, then print
; reshape [2 3] [1 2 3 4 5 6]
```

---

## A complete example - factorial

This program reads a number from standard input and prints its factorial. It uses the bundled `io` library for input.

```
importc "./libraries/io"

factorial => * factorial - 1 . ?< end - 1 .
_(end)
? end 0 ; factorial io.input
```

Save as `factorial.stack` and compile:

```bash
./testlang factorial.stack factorial --exe
./factorial
```

Enter a number when prompted. The program uses a recursive definition: `factorial` pops a value, checks whether it is less than or equal to zero (jumping to `_(end)` if so), and otherwise multiplies it by `factorial` applied to `n - 1`.

---

## Outputting LLVM IR

Passing `--ir` instead of `--exe` writes the LLVM intermediate representation to the output file rather than producing an executable. This is useful for inspecting what the compiler generates.

```bash
./testlang hello.stack hello_ir --ir
```

The file `hello_ir` will contain the raw LLVM IR. You can view it in any text editor, or use the standard LLVM disassembler to annotate it:

```bash
llvm-dis hello_ir -o hello_ir.ll
cat hello_ir.ll
```

> **Note:** Testlang applies a number of LLVM optimisation passes before writing the IR, so the output will be larger than a minimal hand-written equivalent. The user-program logic is present but surrounded by inlined runtime definitions. Use `llvm-dis` and search for your function names to navigate it more easily.

---

## CLI reference

```
./testlang <source.stack> <output> [flags]
```

| Flag | Effect |
|---|---|
| `--exe` | Compile to a standalone native executable (default mode) |
| `--ir` | Write LLVM IR to the output file |
| `--obj` | Write a compiled object file |
| `--exe /path/to/lexer.py` | Use a custom lexer script instead of the bundled one |

Run `./testlang` with no arguments to see usage information.
