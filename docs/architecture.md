# Architecture

## Scope

MiniSIMT `0.4.0` contains two execution layers, shared global memory, and one
observation layer:

1. A scalar `ThreadContext` for focused instruction semantics.
2. A four-lane `Warp` for SIMT execution and active-mask behavior.
3. A word-addressed `GlobalMemory` shared by all active lanes.
4. A `WarpTraceRecord` for state transitions and per-lane memory accesses.

```text
                  +--> ThreadContext --> scalar step()
Program + fetch --|
                  +--> Warp ---------> warp step()
                        |                  |
                        |                  +--> GlobalMemory
                        |
                        +--> shared PC
                        +--> active mask
                        +--> 4 x LaneContext
                        |
                        +--> TraceRecord --> JSON line
```

`Instruction` describes work. A scalar context owns registers, a PC, and
completion state. A warp owns the PC because all lanes fetch one instruction
together. Each `LaneContext` stores only lane-private registers and completion
state. This avoids four duplicated PC values that could become inconsistent.

## Execution Contract

Scalar and warp execution use the same instruction validation rules:

1. A finished context returns `Halted`.
2. An out-of-range PC returns `Error`.
3. Instruction syntax is validated before state changes.
4. Memory instructions require a `GlobalMemory` object.
5. All effective addresses are validated before state changes.
6. A successful data instruction advances the PC exactly once.
7. `EXIT` leaves the PC at the exit instruction and returns `Halted`.
8. Any error leaves architectural state and memory unchanged.

Warp execution adds these rules:

1. A warp with no active lanes returns `Halted`.
2. Only active lanes validate operands and execute.
3. The shared PC advances once, independent of active-lane count.
4. Every active lane is validated before any lane commits.
5. `EXIT` finishes active lanes, clears the mask, and halts the warp.

The current active mask is supplied as architectural state. Conditional
branches will generate and restore masks in the divergence milestone.

## Instruction Set

| Opcode | Semantics |
|---|---|
| `MOV_IMM dst, value` | `R[dst] = value` |
| `MOV_REG dst, src1` | `R[dst] = R[src1]` |
| `ADD dst, src1, src2` | `R[dst] = R[src1] + R[src2]` |
| `MUL dst, src1, src2` | `R[dst] = R[src1] * R[src2]` |
| `LANE_ID dst` | `R[dst] = current lane index`; scalar mode uses zero |
| `LD dst, [base + offset]` | Load one memory word |
| `ST [base + offset], value` | Store one memory word |
| `EXIT` | Halt the scalar context or active warp lanes |

## Global Memory

`GlobalMemory` is a shared vector of integer words. An effective address is:

```text
R[base] + signed immediate offset
```

The result must be in `[0, memory.size())`. Address validation uses a wider
intermediate type so base-plus-offset calculation cannot overflow before the
range check.

Warp stores commit in ascending lane order after all active addresses have
passed validation. This makes same-address stores deterministic without
claiming to model a commercial GPU's race semantics.

## Active Mask Convention

The model uses `std::bitset<4>`. Bit `i` controls lane `i`. Text traces use the
usual most-significant-bit-first form:

```text
0b0101 -> lanes 0 and 2 active
0b1111 -> all four lanes active
0b0000 -> no lane can execute
```

## Trace Contract

`step_with_trace()` calls the normal warp execution path, then records:

- the complete before and after warp snapshots;
- the fetched instruction when fetch occurred;
- the execution result;
- lane, kind, effective address, and value for each successful load/store.

It observes the normal commit path instead of implementing a second
interpreter. See `trace-schema.md` for the JSON contract.

## Planned Extensions

1. Conditional branches, active-mask generation, and reconvergence.
2. Shared memory and barrier semantics.
3. Multiple warps and a deterministic scheduler.
4. Active-lane utilization and memory-access counters.
5. Python differential testing against a scalar reference.
