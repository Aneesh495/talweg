# Talweg Write-Ahead Logging & Crash Recovery Engine

## 1. Storage Consistency and Power Failure Hazards

Solid-state storage systems must guarantee consistency and durability even when electrical power is abruptly interrupted mid-operation (unclean power cuts, host kernel panics, or sudden disconnects). Flash controllers face two critical crash hazards:
1. **Metadata Inconsistency**: Host updates modify volatile in-memory state (L2P table, free block stack, wear heaps) before they are serialized to physical flash.
2. **Torn Writes**: Sudden power cut mid-way through programming a 4 KiB flash page leaves the physical cells in a metastable, indeterminate state with corrupted ECC/CRC checksums.

Talweg addresses these hazards through a dual-mechanism recovery architecture:
- A disk-backed **Write-Ahead Log (WAL) Journal** serializing transactional operations with monotonic sequences.
- An **Out-of-Band (OOB) Flash Media Scanner** capable of cold-rebuilding the full L2P mapping table and block states from physical flash in the absence of a clean journal checkpoint.

---

## 2. Write-Ahead Journal Architecture

The write-ahead journal (`talweg_journal_io.c` and `talweg_journal.dats`) records every transaction before it is committed to volatile mapping structures.

```
+--------------------------------------------------------------------------+
|                     Write-Ahead Journal File Format                      |
|                                                                          |
|  0x00               0x08           0x10               0x20               |
|  +------------------+--------------+------------------+---------------+  |
|  | Magic (0x5457414C)| Version (1)  | Checkpoint Seq   | Reserved      |  |
|  | "TWAL"           |              |                  |               |  |
|  +------------------+--------------+------------------+---------------+  |
|  |<---------------------- Journal Header (32 bytes) ----------------->|  |
|                                                                          |
|  0x20               0x28           0x30               0x40               |
|  +------------------+--------------+------------------+---------------+  |
|  | Record 1: WRITE  | LBA: 12      | PPN: 1045        | CRC32C        |  |
|  +------------------+--------------+------------------+---------------+  |
|  | Record 2: TRIM   | LBA: 99      | PPN: 0xFFFFFFFF  | CRC32C        |  |
|  +------------------+--------------+------------------+---------------+  |
|  | Record 3: ERASE  | Block: 4     | EC: 12           | CRC32C        |  |
|  +------------------+--------------+------------------+---------------+  |
|  | ...              | ...          | ...              | ...           |  |
|  +------------------+--------------+------------------+---------------+  |
|  | Torn Record N    | [Interrupted mid-write by power-loss]           |  |
|  +------------------+-------------------------------------------------+  |
+--------------------------------------------------------------------------+
```

### 2.1 WAL Record Binary Structure (`talweg_wal_record_t`)

Each transaction is recorded in a fixed 32-byte record:

```c
typedef struct {
    uint64_t seq;        // Monotonically increasing sequence number
    uint32_t type;       // Record type: WRITE, TRIM, ERASE, CHECKPOINT
    uint32_t lba;        // Logical Block Address
    uint32_t ppn;        // Physical Page Number
    uint32_t flags;      // Transaction flags
    uint32_t crc;        // CRC32C computed over preceding 28 bytes
} talweg_wal_record_t;
```

### 2.2 Record Types

1. **`TALWEG_WAL_REC_WRITE` (`1`)**: Records that sector `lba` was programmed to physical page `ppn` under sequence `seq`.
2. **`TALWEG_WAL_REC_TRIM` (`2`)**: Records that sector `lba` was unmapped.
3. **`TALWEG_WAL_REC_ERASE` (`3`)**: Records that physical block `lba` was erased and incremented its erase count to `ppn`.
4. **`TALWEG_WAL_REC_CHECKPOINT` (`4`)**: Marks a clean synchronization point where all preceding updates have been committed to flash media.

---

## 3. Crash Recovery State Machine

Upon initialization, the FTL executes a three-phase recovery protocol inspired by the ARIES recovery algorithm:

```
               +--------------------------------------+
               |          Boot & Mount Request        |
               +--------------------------------------+
                                  |
                                  v
               +--------------------------------------+
               |       Phase 1: Media OOB Scan        |
               | - Scans all physical flash pages     |
               | - Validates OOB header CRC32C        |
               | - Reconstructs base L2P map table    |
               | - Reconstructs active block pointer  |
               +--------------------------------------+
                                  |
                                  v
               +--------------------------------------+
               |    Phase 2: Journal Forward Scan     |
               | - Reads WAL records sequentially     |
               | - Validates record CRC32C checksum   |
               | - Checks monotonic sequence numbers  |
               +--------------------------------------+
                                  |
                                  +------------+
                                  |            |
                      Valid CRC   |            | Torn Tail / Invalid CRC
                                  v            v
               +--------------------+   +------------------------------------+
               | Redo Transaction   |   | Phase 3: Torn Tail Truncation      |
               | - Update L2P table |   | - Discard corrupted tail bytes     |
               | - Advance sequence |   | - Truncate log to last valid record|
               +--------------------+   +------------------------------------+
                                  |            |
                                  +-----+------+
                                        |
                                        v
               +--------------------------------------+
               |         Recovery Complete            |
               | Engine Enters Normal Service Loop    |
               +--------------------------------------+
```

### 3.1 Phase 1: Physical Media OOB Scan
If the journal log is absent or recovering from cold storage, the FTL scans each block and page in the flash device. Every page programmed by Talweg contains an Out-of-Band (OOB) spare area with:
- `oob.lba`: The owning logical block address.
- `oob.seq`: The write sequence number.
- `oob.crc_header`: Castagnoli CRC32C protecting the OOB metadata.

The scanner verifies `oob.crc_header`. For each valid page, if its `seq` is newer than any previously seen page for that `lba`, the L2P table is updated with `ppn`. The block allocator is updated: fully programmed blocks are marked closed, partially programmed blocks become the open active block, and all untouched blocks are restored to the free stack.

### 3.2 Phase 2: Journal Forward Replay (Redo Pass)
The recovery scanner opens the journal log file, validates the 32-byte journal header magic (`0x5457414C`), and scans forward:
- For each record, the scanner recomputes the CRC32C over the first 28 bytes and compares it to `rec.crc`.
- Verifies that `rec.seq` strictly exceeds `last_valid_seq`.
- Applies the record:
  - If `WRITE`: updates `L2P[rec.lba] = rec.ppn`.
  - If `TRIM`: unmaps `rec.lba` (`L2P[rec.lba] = TALWEG_UNMAPPED_PPN`).
  - If `ERASE`: updates block erase count in wear tracker.

### 3.3 Phase 3: Torn Tail Handling
If a power interruption occurred mid-write, the final journal record will be partially written or contain invalid checksum bytes. When the forward scanner encounters a record failing the CRC32C verification:
1. Replay terminates immediately at the boundary of the last valid record.
2. Corrupted tail bytes are safely discarded without affecting any previously committed transactions.
3. The journal file is truncated to the byte offset of the last sound transaction, restoring crash consistency.

---

## 4. Verification and Monte Carlo Crash Drills

Talweg includes a built-in Monte Carlo crash drill tool (`bin/talweg crash-drill --cycles N`) and automated Python test suites (`tests/test_crash_recovery.py`):
- Executes hundreds of live storage transactions against active flash media.
- Injects abrupt `SIGKILL` termination signals to simulate instantaneous power cuts without clean flush.
- Restarts new server instances on the ungracefully closed media and verifies that every single block matches pre-crash SHA-256 golden references bit-for-bit.
- Appends randomized garbage bytes to journal files to verify that torn tails are detected and truncated without crashing.
