# Verification Plan

## Model Contract

Verification checks final values and state transitions. A failed instruction
must not partially update registers, the PC, active mask, completion state, or
global memory.

## Scalar Test Matrix

| Category | Case | Expected result |
|---|---|---|
| Data | Immediate and register moves | Destination updated, `pc + 1` |
| Data | Addition and multiplication | Result written to destination |
| Built-in | `LANE_ID` | Scalar lane index is zero |
| Memory | `LD` with positive offset | Selected word loaded |
| Memory | `ST` with positive offset | Selected word stored |
| Memory | `LD` without memory object | `Error`, no state change |
| Lifecycle | `EXIT` and step after exit | Context remains halted |
| Invalid | Register index 8 | `Error`, no state change |
| Invalid | Malformed instruction | `Error`, no state change |
| Invalid | PC out of range | `Error`, no state change |
| Integration | Register and memory program | Deterministic final state |

## Warp Test Matrix

| Category | Case | Expected result |
|---|---|---|
| SIMT | Four-lane `ADD` and `MUL` | Lane-private results |
| Built-in | Four-lane `LANE_ID` | Registers contain 0, 1, 2, 3 |
| Shared state | Four active lanes | Shared PC advances exactly once |
| Mask | `0b0101` data instruction | Only lanes 0 and 2 execute |
| Memory | Masked `LD` | Inactive invalid addresses are ignored |
| Memory | One active out-of-range `ST` | No lane or memory update |
| Lifecycle | Warp `EXIT` | Active lanes finish and mask clears |
| Boundary | Empty active mask | `Halted`, no state change |
| Invalid | Finished lane remains active | `Error`, no state change |
| Integration | Vector-add kernel | Output is 11, 22, 33, 44 |

## Trace Test Matrix

| Category | Case | Expected result |
|---|---|---|
| Snapshot | Traced `MUL` | Complete before/instruction/result/after |
| Memory | Masked `LD` | One access record per active lane |
| Fetch error | PC out of range | `instruction=null`, before equals after |
| Serialization | Traced `EXIT` | One valid JSON line |
| Serialization | Traced `LD` | Lane, kind, address, and value serialized |

## Future Test Layers

- Branch divergence and reconvergence traces.
- Shared-memory and barrier ordering.
- Multi-warp scheduling fairness and determinism.
- Python CPU reference and trace diff.
- Regression cases for every discovered defect.
- Metrics tests for active-lane utilization and memory access patterns.
