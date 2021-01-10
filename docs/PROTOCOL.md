# Talweg Binary IPC Wire Protocol Specification

## 1. Protocol Architecture & Framing

Talweg employs a compact, low-overhead binary IPC protocol operating over standard POSIX Unix Domain Sockets (`AF_UNIX`, `SOCK_STREAM`). The wire protocol is designed for high-throughput, low-latency communication between host applications and the flash translation layer controller daemon.

### 1.1 Endianness and Alignment

All multibyte integers transmitted over the wire are encoded in **little-endian** byte order (standard x86_64 and ARM64 native representation). Packet headers are fixed at 36 bytes and are aligned to natural field boundaries to eliminate CPU unaligned access penalties.

### 1.2 Binary Header Layout (`talweg_proto_hdr_t`)

Every request and response frame begins with a fixed 36-byte header:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Magic (0x54574750)                      |  [0x00]
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Command ID           |             Flags             |  [0x04]
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                        Tag / Request ID                       |  [0x08]
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Logical Block Address (LBA)                 |  [0x10]
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          Page Count                           |  [0x14]
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Status Code                           |  [0x18]
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Payload Length                         |  [0x1C]
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        CRC32C Checksum                        |  [0x20]
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 1.3 Field Descriptions

| Field | Offset | Size | Type | Description |
|---|---|---|---|---|
| `magic` | 0 | 4 bytes | `uint32_t` | Protocol magic constant `0x54574750` (ASCII `"TWGP"`). |
| `cmd` | 4 | 2 bytes | `uint16_t` | Command identifier (see Section 2). |
| `flags` | 6 | 2 bytes | `uint16_t` | Bitmask: `0x0001` (Response), `0x0002` (Encrypted), `0x0004` (Direct I/O). |
| `tag` | 8 | 8 bytes | `uint64_t` | Opaque correlation token echoed verbatim in response frame. |
| `lba` | 16 | 4 bytes | `uint32_t` | Target starting Logical Block Address. |
| `count` | 20 | 4 bytes | `uint32_t` | Number of consecutive 4 KiB sectors targeted (typically 1). |
| `status` | 24 | 4 bytes | `int32_t` | Execution status: 0 for OK, negative for errors. |
| `payload_len` | 28 | 4 bytes | `uint32_t` | Byte length of trailing payload immediately following header. |
| `crc` | 32 | 4 bytes | `uint32_t` | CRC32C (Castagnoli) checksum computed over payload bytes. |

---

## 2. Command Reference

### 2.1 PING (`0x0001`)
- **Purpose**: Verifies daemon availability and IPC latency.
- **Request**: `lba = 0`, `count = 0`, `payload_len = 0`.
- **Response**: `status = 0`, `payload_len = 0`.

### 2.2 READ (`0x0002`)
- **Purpose**: Reads one or more 4 KiB logical sectors from the storage engine.
- **Request**: `lba = target_lba`, `count = num_pages`, `payload_len = 0`.
- **Response**:
  - If success: `status = 0`, `payload_len = count * 4096`, payload contains sector data. Unmapped sectors return zero-filled bytes.
  - If error: `status < 0` (e.g. `TALWEG_ERR_OUT_OF_BOUNDS`), `payload_len = 0`.

### 2.3 WRITE (`0x0003`)
- **Purpose**: Programs one or more 4 KiB logical sectors out-of-place.
- **Request**: `lba = target_lba`, `count = num_pages`, `payload_len = count * 4096`, payload contains data to write.
- **Response**: `status = 0`, `payload_len = 0`.

### 2.4 TRIM (`0x0004`)
- **Purpose**: Unmaps one or more logical sectors, releasing flash space and converting subsequent reads to zero-fill.
- **Request**: `lba = target_lba`, `count = num_pages`, `payload_len = 0`.
- **Response**: `status = 0`, `payload_len = 0`.

### 2.5 FLUSH (`0x0005`)
- **Purpose**: Synchronizes all dirty cache buffer frames, commits the write-ahead journal log to disk, and executes `msync()` on physical media.
- **Request**: `lba = 0`, `count = 0`, `payload_len = 0`.
- **Response**: `status = 0`, `payload_len = 0`.

### 2.6 STATS (`0x0006`)
- **Purpose**: Queries live FTL telemetry, wear-leveling metrics, and endurance counters.
- **Request**: `lba = 0`, `count = 0`, `payload_len = 0`.
- **Response**: `status = 0`, `payload_len = sizeof(talweg_proto_stats_payload_t)` (56 bytes).
- **Telemetry Payload Layout**:
  ```c
  typedef struct {
      uint64_t user_writes;      // Total 4KiB sectors written by host
      uint64_t flash_writes;     // Total physical flash pages programmed
      uint64_t gc_cycles;        // Total garbage collection passes executed
      uint32_t waf_mille;        // WAF scaled by 1000 (e.g. 1350 = 1.35 WAF)
      uint32_t free_blocks;      // Count of erased blocks in free pool
      uint32_t min_erase;        // Minimum erase count across flash blocks
      uint32_t max_erase;        // Maximum erase count across flash blocks
      uint32_t wear_delta;       // Current wear delta: max_erase - min_erase
      uint32_t bad_blocks;       // Retired defective physical blocks
      uint32_t mapped_lba_count; // Number of active logical sectors mapped
  } talweg_proto_stats_payload_t;
  ```

### 2.7 GC (`0x0007`)
- **Purpose**: Explicitly triggers an on-demand garbage collection compaction step.
- **Request**: `lba = 0`, `count = 0`, `payload_len = 0`.
- **Response**: `status = 0` (or `TALWEG_ERR_NOT_FOUND` if no blocks require compaction).

---

## 3. Error Codes

| Code | Value | Name | Description |
|---|---|---|---|
| `0` | 0 | `TALWEG_OK` | Operation completed successfully. |
| `-1` | -1 | `TALWEG_ERR_OUT_OF_BOUNDS` | LBA or PPN exceeds media geometry limits. |
| `-2` | -2 | `TALWEG_ERR_MEDIA_IO` | Physical file I/O error or failure during `msync()`. |
| `-3` | -3 | `TALWEG_ERR_NO_FREE_BLOCKS` | Storage exhaustion; all blocks full and GC cannot reclaim space. |
| `-4` | -4 | `TALWEG_ERR_WRITE_PROTECT` | Media mounted in read-only mode. |
| `-5` | -5 | `TALWEG_ERR_PAGE_ALREADY_WRITTEN` | Attempted in-place overwrite on a non-erased flash page. |
| `-6` | -6 | `TALWEG_ERR_CORRUPT_METADATA` | Invalid superblock or corrupted journal header detected. |
| `-7` | -7 | `TALWEG_ERR_BAD_BLOCK` | Targeted block marked defective in Bad Block Table. |
| `-8` | -8 | `TALWEG_ERR_POWER_LOSS` | Unclean shutdown flag detected during mount. |
| `-9` | -9 | `TALWEG_ERR_UNMAPPED` | Sector is unmapped (used internally; reads return zero fill). |
| `-10` | -10 | `TALWEG_ERR_INVAL` | Invalid argument, null pointer, or malformed protocol frame. |
| `-11` | -11 | `TALWEG_ERR_NOMEM` | Out of host memory during buffer allocation. |
| `-12` | -12 | `TALWEG_ERR_NOT_FOUND` | Target record or victim candidate not found. |
