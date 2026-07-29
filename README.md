# MiniSIMT

MiniSIMT is a small C++20 functional model for learning GPU Core execution
semantics and architecture verification. It executes a deliberately tiny ISA,
models a four-lane SIMT warp, and emits deterministic state and memory traces.

## Current Status

Version `0.4.0` can execute a complete four-element vector-add kernel:

- A `Warp` owns one shared program counter and four lane contexts.
- Each lane owns private integer registers and completion state.
- A four-bit active mask selects which lanes execute an instruction.
- `LANE_ID` exposes the physical lane index to the program.
- `LD` and `ST` access a shared, word-addressed `GlobalMemory`.
- `MOV_IMM`, `MOV_REG`, `ADD`, and `MUL` provide integer data operations.
- `EXIT` finishes active lanes and halts the warp.
- All active-lane operands and addresses are validated before commit, so one
  invalid lane cannot cause partial register or memory updates.
- JSON Lines traces include complete before/after warp state and one memory
  access record per active lane.
- Scalar `ThreadContext` execution remains available for focused instruction
  tests.

Conditional branches, divergence and reconvergence, shared memory, multi-warp
scheduling, and performance counters are planned follow-up milestones.

## Build and Test

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
.\build\minisimt_demo.exe
```

On Linux or macOS, run `./build/minisimt_demo` for the final command.

Expected demo output:

```text
MiniSIMT four-lane vector-add kernel
A = [1, 2, 3, 4]
B = [10, 20, 30, 40]
C = [11, 22, 33, 44]
status = PASS
```

Use `minisimt_demo --trace` to emit one compact JSON object per step.

## Vector-Add Kernel

The demo stores `A`, `B`, and `C` in three contiguous four-word memory regions.
Each lane computes one element:

```text
LANE_ID R0
LD      R1, [R0]
LD      R2, [R0 + 4]
ADD     R3, R1, R2
ST      [R0 + 8], R3
EXIT
```

`LANE_ID` gives lanes 0 through 3 different addresses while all lanes fetch the
same instruction from the shared PC. This is the smallest end-to-end example
that demonstrates the difference between SIMT execution and four independent
scalar interpreters.

## Memory Model

`GlobalMemory` is a vector of integer words. `LD` and `ST` use base-plus-offset
addressing:

```text
LD Rdst, [Rbase + offset]
ST [Rbase + offset], Rvalue
```

Addresses are word indexes, not byte addresses. Negative and out-of-range
addresses return `Error`. Before any lane commits an instruction, MiniSIMT
validates every active lane. Inactive lanes neither validate nor access memory.
When multiple active lanes store to the same address, ascending lane order
defines the deterministic final value.

## Active Mask Convention

Traces print mask bits in lane order 3 through 0, where the rightmost bit is
lane 0. For example, `0b0101` activates lanes 0 and 2. An inactive lane does
not change registers or access memory, while the warp still advances its
single shared PC.

## Structured Trace

`step_with_trace()` stores:

- the complete warp state before execution;
- the fetched instruction, or `null` if fetch did not occur;
- the `Executed`, `Halted`, or `Error` result;
- one load/store record per active lane;
- the complete warp state after execution.

See [`docs/trace-schema.md`](docs/trace-schema.md) for the field contract.

## Project Boundaries

MiniSIMT is an educational functional model. It does not implement a commercial
GPU ISA, cycle-accurate timing, a compiler, a driver, a runtime, caches, or RTL
behavior. See [`docs/architecture.md`](docs/architecture.md) and
[`docs/verification-plan.md`](docs/verification-plan.md).
