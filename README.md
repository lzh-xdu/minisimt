# MiniSIMT

MiniSIMT is a small C++20 functional model for learning GPU Core execution
semantics and architecture verification. It executes a deliberately tiny
instruction set and produces deterministic state traces.

## Current Status

Version `0.1.0` models one scalar thread context:

- `MOV_IMM`: move an immediate value into a register.
- `MOV_REG`: copy one register into another.
- `ADD`: add two source registers.
- `EXIT`: halt the thread.
- Eight integer registers, a program counter, and thread completion state.
- Operand validation with an atomic error contract: failed instructions do not
  change architectural state.
- Automated normal, boundary, malformed-instruction, and program-flow tests.

Warp execution, active masks, memory instructions, branch divergence, Python
golden references, and performance metrics are planned follow-up milestones.

## Build and Test

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
.\build\minisimt_demo.exe
```

On Linux or macOS, run `./build/minisimt_demo` for the final command.

## Example Program

```text
MOV_IMM R0, 10
MOV_IMM R1, 20
ADD R2, R0, R1
MOV_REG R3, R2
EXIT
```

Expected final state:

```text
R0=10 R1=20 R2=30 R3=30 finished=true
```

## Project Boundaries

MiniSIMT is an educational functional model. It does not implement a commercial
GPU ISA, cycle-accurate timing, a compiler, a runtime, or RTL behavior. See
[`docs/architecture.md`](docs/architecture.md) and
[`docs/verification-plan.md`](docs/verification-plan.md).
