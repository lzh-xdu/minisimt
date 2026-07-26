# Architecture

## Scope

MiniSIMT `0.1.0` is a scalar functional model. One call to `step()` executes
exactly one instruction at the current program counter.

```text
Program + ThreadContext
          |
          v
        step()
          |
          +--> StepResult
          +--> updated ThreadContext
```

`Instruction` describes work. `ThreadContext` stores architectural state:
registers, the program counter, and whether the thread has halted.

## Execution Contract

1. A halted context returns `Halted`.
2. An out-of-range program counter returns `Error`.
3. The instruction and all operands are validated before state changes.
4. A successful data instruction writes its result, increments `pc`, and
   returns `Executed`.
5. `EXIT` marks the context as finished and returns `Halted`.
6. Any error leaves all architectural state unchanged.

## Instruction Set

| Opcode | Semantics |
|---|---|
| `MOV_IMM dst, value` | `R[dst] = value` |
| `MOV_REG dst, src1` | `R[dst] = R[src1]` |
| `ADD dst, src1, src2` | `R[dst] = R[src1] + R[src2]` |
| `EXIT` | Halt the current thread |

## Planned Extension

The next model layer groups multiple `ThreadContext` instances into a `Warp`
with a shared program counter and active mask. Later milestones add global
memory, `LD`/`ST`, conditional branches, divergence, deterministic trace
records, and Python differential testing.
