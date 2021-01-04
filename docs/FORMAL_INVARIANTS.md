# Formal Verification, Dependent Types, and Linear Safety Invariants in ATS

## 1. The Applied Type System (ATS2) Architecture

Talweg leverages ATS2 (Applied Type System - Postiats) to establish mathematical proofs of structural invariants, physical flash boundaries, and memory safety directly within the type system. Unlike mainstream languages where safety properties are verified via dynamic assertions or post-hoc runtime checking, ATS compiles static proofs into zero-cost C code, guaranteeing at compile time that critical storage engine invariants can never be violated.

### 1.1 Separation of Statics and Dynamics

ATS2 strictly delineates two distinct computational domains:
1. **The Static Domain (Statics)**: A purely functional, terminating specification language where types, sorts, and propositions live. Code in the static domain is evaluated exclusively by the type-checker and is completely erased during compilation to C.
2. **The Dynamic Domain (Dynamics)**: The runtime execution layer containing standard imperative procedures, linear pointer manipulation, and operating system I/O.

By connecting dynamic values to static index terms via dependent types (`{x: nat | x < N} int(x)`), the type checker enforces that dynamic computations conform to mathematical properties established in the static domain.

```
+--------------------------------------------------------------------------+
|                      ATS Proof and Verification Flow                     |
|                                                                          |
|  Static Domain (Proof Engine)            Dynamic Domain (Runtime)        |
|  ============================            ========================        |
|  Sorts: nat, addr, lba_sort              Values: uint32, pointers        |
|  Props: LT(i, N), L2P_VALID(m, lba)      Functions: lba_map_update()     |
|  Proofs: prfun lemma_bounds()            State: Hardware Media I/O       |
|               |                                      |                   |
|               +------------------+-------------------+                   |
|                                  |                                       |
|                                  v                                       |
|                    patsopt (Static Type Checker)                         |
|                    Enforces Invariants & Erases Proofs                   |
|                                  |                                       |
|                                  v                                       |
|                     Pure C17 Compilation Target                          |
|                     (Zero Runtime Proof Overhead)                        |
+--------------------------------------------------------------------------+
```

---

## 2. Coordinate Geometry and Boundary Sorts

NAND flash physical addressing follows rigid geometric constraints. A single out-of-bounds page index can trigger undefined hardware behavior, program disturbs, or bus lockups.

In `src/ats/SATS/talweg_geom.sats`, Talweg defines static sorts and dependent types for each geometric tier:

```ats
sortdef channel_sort = {c: int | 0 <= c && c < 4}
sortdef die_sort     = {d: int | 0 <= d && d < 2}
sortdef plane_sort   = {p: int | 0 <= p && p < 2}
sortdef block_sort   = {b: int | 0 <= b && b < 1024}
sortdef page_sort    = {p: int | 0 <= p && p < 256}
sortdef ppn_sort     = {ppn: int | 0 <= ppn && ppn < 524288}
sortdef lba_sort     = {lba: int | 0 <= lba && lba < 262144}
```

### 2.1 Static Boundary Predicates

To guarantee that decomposing a flat physical page number ($PPN$) into channel, die, plane, block, and page coordinates produces valid values, Talweg declares dependent constructor functions:

```ats
fun geom_decompose_ppn {ppn: ppn_sort} (
  ppn: int(ppn),
  total_blks: uint,
  ppb: uint
): [c: channel_sort] [d: die_sort] [p: plane_sort] [b: block_sort] [pg: page_sort] (
  int(c), int(d), int(p), int(b), int(pg)
)
```

The static quantifier `[c: channel_sort]` asserts that the returned channel coordinate is provably within `[0, 3]`. Any attempt in dynamic code to access a channel outside this range fails compilation with a type error.

---

## 3. Physical Flash Page State Machine via Linear Views

In raw NAND flash, a physical page can only be programmed once per erase cycle. Programming an already-written page without an intervening block erase induces physical write disturbs and data corruption.

Talweg formalizes the physical state machine of every flash page using ATS linear views (`view`):

```
+-------------------------------------------------------------------------+
|                  Linear Page State Transition Automaton                 |
|                                                                         |
|            +--------------+          talweg_media_prog_page             |
|            |    ERASED    | ----------------------------------> +       |
|            +--------------+                                     |       |
|                   ^                                             v       |
|                   |                                    +--------------+ |
| talweg_block_erase|                                    |  PROGRAMMED  | |
|                   |                                    +--------------+ |
|                   |                                             |       |
|            +--------------+       Host Overwrite / Trim         |       |
|            |    DIRTY     | <-----------------------------------+       |
|            |  (INVALID)   |                                             |
|            +--------------+                                             |
+-------------------------------------------------------------------------+
```

### 3.1 Linear Resource Types and View Proofs

In ATS, a linear view represents unforgeable ownership of a resource. Linear views must be consumed exactly once, preventing double-program bugs:

```ats
dataprop PAGE_STATE (ppn: int, state: int) =
  | PAGE_ERASED (ppn, 0) of ()
  | PAGE_PROGRAMMED (ppn, 1) of ()
  | PAGE_INVALID (ppn, 2) of ()

(* Transition from ERASED to PROGRAMMED consumes ERASED view *)
prfun page_program_transition {ppn: ppn_sort} (
  pf: PAGE_ERASED(ppn, 0)
): PAGE_PROGRAMMED(ppn, 1)

(* Transition from PROGRAMMED to INVALID upon overwrite *)
prfun page_invalidate_transition {ppn: ppn_sort} (
  pf: PAGE_PROGRAMMED(ppn, 1)
): PAGE_INVALID(ppn, 2)
```

Because `page_program_transition` requires a proof of `PAGE_ERASED(ppn, 0)`, the dynamic program function cannot be called on a page that is in the `PAGE_PROGRAMMED` or `PAGE_INVALID` state. Attempting to program a page twice is statically rejected by the compiler.

---

## 4. Logical-to-Physical (L2P) Mapping Invariants

The Logical-to-Physical mapping table forms the critical routing layer of the FTL. Talweg specifies and enforces three formal invariants:

1. **Mapping Totality**: Every $LBA \in [0, \text{max\_lba} - 1]$ maps either to a valid $PPN \in [0, \text{total\_pages} - 1]$ or to the sentinel $\text{TALWEG\_UNMAPPED\_PPN} = 0xFFFFFFFF$.
2. **Reverse Mapping Consistency**: If $L2P[lba] = ppn \neq \bot$, then the physical page at $ppn$ has an OOB spare header where $\text{oob.lba} = lba$.
3. **No Alias Invariant**: No two distinct logical addresses $lba_1 \neq lba_2$ may simultaneously map to the same active physical page $ppn$:
   $$\forall lba_1, lba_2: lba_1 \neq lba_2 \implies (L2P[lba_1] = \bot \lor L2P[lba_2] = \bot \lor L2P[lba_1] \neq L2P[lba_2])$$

In `src/ats/SATS/talweg_lba_map.sats`, these invariants are captured in the linear mapping table type:

```ats
absvtype lba_map_table_vtype = ptr

fun lba_map_lookup {lba: lba_sort} (
  tbl: !lba_map_table_vtype,
  lba: int(lba)
): [ppn: int | ppn == 0xFFFFFFFF || (0 <= ppn && ppn < 524288)] uint(ppn)
```

---

## 5. Wear-Leveling Delta Bound Proof

Talweg enforces a bounded wear invariant:

$$\Delta(t) = \max_{i} EC_i(t) - \min_{j} EC_j(t) \le \Delta_{\text{thresh}}$$

In `src/ats/SATS/talweg_wear.sats`, the wear-leveling heap maintains two linear priority queues:
- `min_heap`: Indexes erased blocks prioritized by ascending erase count.
- `max_heap`: Indexes active blocks prioritized by descending erase count.

### 5.1 Wear Invariant Maintenance

Whenever an allocation or block erase modifies an erase count:
1. The new erase count $EC_{\text{new}}$ is inserted into both heaps.
2. The controller computes $\Delta = \text{peek}(\text{max\_heap}) - \text{peek}(\text{min\_heap})$.
3. If $\Delta > \Delta_{\text{thresh}}$, the proof engine forces the invocation of `talweg_wear_relocate_cold_data()`, migrating cold static data into the high-wear block and releasing the low-wear block to absorb future writes.

---

## 6. Page Buffer Cache Linear Pinned Views

Concurrent storage systems frequently suffer from buffer management defects, including buffer leaks, dangling pointer dereferences after eviction, and race conditions.

Talweg solves this by modeling cache frames as linear view-types (`vtype`):

```ats
absvtype cache_frame_vtype = ptr

fun cache_pin (
  cache: !cache_pool_vtype,
  lba: uint,
  out_buf: &ptr? >> ptr
): [rc: int | rc == 0 || rc == -3] int(rc)

fun cache_unpin (
  cache: !cache_pool_vtype,
  lba: uint,
  dirty: bool
): void
```

The ATS linear type system enforces:
- **Pin/Unpin Balancing**: A pinned frame represents an active linear capability. A caller holding a pinned frame cannot terminate without calling `cache_unpin`, preventing frame starvation and resource leaks.
- **Eviction Safety**: The LRU eviction engine cannot evict any frame whose pin count is greater than zero ($pin\_count > 0$).
