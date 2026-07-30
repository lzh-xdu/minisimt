# Trace Schema

MiniSIMT emits one compact JSON object per warp step. The format is JSON Lines:
each line is independently parseable and contains no embedded newline.

## Top-Level Fields

| Field | Type | Meaning |
|---|---|---|
| `result` | string | `Executed`, `Halted`, or `Error` |
| `instruction` | object or null | Instruction fetched at the shared PC |
| `memory_accesses` | array | Successful per-lane loads or stores |
| `before` | warp object | Complete architectural state before the step |
| `after` | warp object | Complete architectural state after the step |

`instruction` is `null` when execution stops before fetch, including an
out-of-range PC, a finished warp, or an empty active mask.

## Instruction Object

| Field | Type |
|---|---|
| `opcode` | string |
| `dst` | integer |
| `src1` | integer |
| `src2` | integer |
| `immediate` | integer |
| `target` | integer |
| `reconverge` | integer |

Unused operands retain the instruction model's sentinel values.

## Memory Access Object

| Field | Type | Meaning |
|---|---|---|
| `lane` | non-negative integer | Lane that performed the access |
| `kind` | string | `load` or `store` |
| `address` | non-negative integer | Effective word address |
| `value` | integer | Loaded or stored value |

The array is empty for non-memory instructions and failed instructions. Accesses
are ordered by ascending lane index.

## Warp Object

| Field | Type | Meaning |
|---|---|---|
| `pc` | non-negative integer | Shared warp program counter |
| `finished` | boolean | Warp completion state |
| `active_mask` | four-character string | Bits for lanes 3 through 0 |
| `reconvergence_stack` | array | Deferred divergent paths |
| `lanes` | array of four lane objects | Lane-private state |

Each lane object contains `finished` and an array of eight integer registers.

## Reconvergence Frame Object

| Field | Type | Meaning |
|---|---|---|
| `reconverge_mask` | four-character string | Mask active before divergence |
| `pending_mask` | four-character string | Lanes assigned to the deferred path |
| `pending_pc` | non-negative integer | Deferred path entry PC |
| `reconverge_pc` | non-negative integer | PC where both paths merge |
| `pending_path_started` | boolean | Whether execution switched to the deferred path |

## Illustrative Shape

```text
{
  "result": "Executed",
  "instruction": {
    "opcode": "LD",
    "dst": 1,
    "src1": 0,
    "src2": -1,
    "immediate": 4,
    "target": -1,
    "reconverge": -1
  },
  "memory_accesses": [
    {"lane": 0, "kind": "load", "address": 4, "value": 10}
  ],
  "before": {
    "pc": 0,
    "finished": false,
    "active_mask": "0001",
    "reconvergence_stack": [],
    "lanes": [four complete lane snapshots]
  },
  "after": {
    "pc": 1,
    "finished": false,
    "active_mask": "0001",
    "reconvergence_stack": [],
    "lanes": [four complete lane snapshots]
  }
}
```

The actual output is compact valid JSON on one line. The expanded shape above
is documentation, not a literal output sample.
