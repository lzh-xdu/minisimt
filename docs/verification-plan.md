# Verification Plan

## Model Contract

Verification checks both final values and state transitions. A failed
instruction must not partially update registers, the program counter, or thread
completion state.

## Current Test Matrix

| Category | Case | Expected result |
|---|---|---|
| Normal | Immediate move | Destination updated, `pc + 1` |
| Boundary | Negative immediate | Value remains a valid immediate |
| Normal | Register move | Source copied to destination |
| Normal | Addition | Sum written to destination |
| Lifecycle | Exit | Thread becomes halted |
| Lifecycle | Step after exit | Thread remains halted |
| Invalid | Register index 8 | `Error`, no state change |
| Invalid | Malformed MOV operands | `Error`, no state change |
| Invalid | Program counter out of range | `Error`, no state change |
| Integration | MOV/MOV/ADD/MOV/EXIT | Deterministic final state |

## Planned Test Layers

- Per-opcode unit tests.
- Thread and Warp boundary tests.
- Load/store address and alignment tests.
- Branch divergence and reconvergence traces.
- Python CPU reference and trace diff.
- Regression cases for every discovered defect.
