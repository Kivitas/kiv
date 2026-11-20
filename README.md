# kiv

A small scripting language with a bytecode VM, a mark-and-sweep garbage collector,
and an x86-64 JIT for integer expressions.

Copyright (C) 2026 kivitas. Licensed under the GNU General Public License,
version 3 or later. See [LICENSE](LICENSE).

## Layout

| Path | Language | Role |
| --- | --- | --- |
| `frontend/` | Rust | Lexer, parser, AST, bytecode compiler. Exposed as a C ABI (`kiv_compile`). |
| `runtime/` | C++17 | Value representation, mark-and-sweep heap, stack interpreter. |
| `jit/` | C++17 + x86-64 assembly | Emits x86-64 machine code into executable pages. A trampoline enters and leaves it. |
| `include/kiv/` | C / C++ | Shared bytecode definitions and the frontend C header. |
| `src/` | C++17 | Command-line driver. |
| `tests/` | sh | Example scripts with expected output, plus error-exit checks. |

The Rust frontend is linked into the C++ binary as a static library. The C ABI
is intentionally small, so the boundary is two functions and a buffer type.

## Language (v0)

```
# comments run to end of line
print 1 + 2 * 3;          # integers: + - * with usual precedence, left-associative
print (1 + 2) * 3;        # parentheses
print "hello, " + "world"; # strings: literals and + (concatenation)
```

Integer arithmetic wraps on overflow. Mixing types, for example `1 + "a"`, is a
runtime error (exit status 2). Variables, control flow and functions are not
implemented yet.

## Building

Requirements: GNU make, a C++17 compiler, and a Rust toolchain with `cargo`.
The Rust frontend has no external crates, so no network access is needed.

```bash
make            # builds build/kiv
make test       # cargo tests, then the example scripts
make clean
```

Run a script:

```bash
./build/kiv tests/examples/arith.kiv
./build/kiv --jit tests/examples/arith.kiv       # native code for integer expressions
./build/kiv --gc-stress --gc-stats tests/examples/gc.kiv
```

## Status

- **Frontend (Rust):** done. Unit tests cover precedence, bytecode layout,
  error positions, and a check that the opcode values match the C header.
- **Runtime (C++):** interpreter and collector work. The collector is non-moving
  and runs before allocation. Strings are the only heap objects, so tracing has
  no child references yet.
- **JIT:** x86-64 Linux only. It compiles runs of `PUSH_INT`, `ADD`, `SUB` and
  `MUL` that end in a `PRINT`, and it keeps executable pages write-xor-execute.
  Anything else falls back to the interpreter.
- **ARM64:** not implemented. The build disables the JIT on other architectures,
  and `--jit` then reports that it is unavailable.

## Design notes and next steps

- The JIT compiles on first execution. A hotness counter (compile after N runs)
  is the natural next step, and it would make the JIT pay off on loops.
- Variables and loops are needed before the JIT can handle anything beyond
  straight-line code. Their addition would also give the collector real
  object graphs to trace.
- The C ABI was chosen over the `cxx` crate to keep the boundary dependency-free.
  Moving to `cxx` later would only touch `frontend/src/lib.rs` and `include/kiv/frontend.h`.
