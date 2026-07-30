# MiniSIMT

MiniSIMT is a small C++20 functional model for learning GPU Core execution
semantics and architecture verification. It executes a deliberately tiny ISA,
models a four-lane SIMT warp, and emits deterministic state and memory traces.

## Current Status

Version `0.5.0` can execute a complete four-element vector-add kernel and a
structured divergent branch:

- A `Warp` owns one shared program counter and four lane contexts.
- Each lane owns private integer registers and completion state.
- A four-bit active mask selects which lanes execute an instruction.
- `CMP_LT` creates one predicate value per lane.
- `BRA_IF` splits active lanes, and a reconvergence frame preserves the deferred
  path and the mask to restore.
- `JUMP` ends the fallthrough path at the declared reconvergence PC.
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

The branch model is intentionally limited to one forward, structured divergence
at a time. Nested divergent branches and early `EXIT` inside a divergent region
return `Error`. Shared memory, multi-warp scheduling, and performance counters
remain follow-up milestones.

## Build and Test

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
.\build\minisimt_demo.exe
.\build\minisimt_demo.exe --branch
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

The branch demo prints:

```text
MiniSIMT four-lane branch divergence
predicate = [1, 1, 0, 0]
path_value = [100, 100, 200, 200]
merged = [100, 101, 202, 203]
status = PASS
```

Use `minisimt_demo --trace` or `minisimt_demo --branch --trace` to emit one
compact JSON object per step.

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

## Branch Divergence

The branch demo divides lanes 0-1 from lanes 2-3:

```text
LANE_ID R0
MOV_IMM R1, 2
CMP_LT  R2, R0, R1
BRA_IF  R2, taken=6, reconverge=7
MOV_IMM R3, 200
JUMP    7
MOV_IMM R3, 100
ADD     R4, R3, R0
EXIT
```

For a mixed predicate, MiniSIMT runs the fallthrough lanes first and stores the
taken lanes in a `ReconvergenceFrame`. Reaching PC 7 switches to the deferred
taken path. Reaching PC 7 again restores the original mask, so all four lanes
execute the final `ADD`.

The instruction immediately before the taken target must be
`JUMP reconverge`. While a reconvergence frame is active, any `JUMP` to another
target returns `Error`. This keeps the milestone's control flow structured and
prevents a path from silently skipping its merge point.

The active-mask sequence is:

```text
1111 -> 1100 -> 0011 -> 1111 -> 0000
```

This serializes divergent paths while preserving the GPU-style abstraction of
one PC per warp instead of one PC per lane.

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
- the complete reconvergence stack before and after the step;
- the complete warp state after execution.

See [`docs/trace-schema.md`](docs/trace-schema.md) for the field contract.

## Project Boundaries

MiniSIMT is an educational functional model. It does not implement a commercial
GPU ISA, cycle-accurate timing, a compiler, a driver, a runtime, caches, or RTL
behavior. See [`docs/architecture.md`](docs/architecture.md) and
[`docs/verification-plan.md`](docs/verification-plan.md).
