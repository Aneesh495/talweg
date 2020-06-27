(*
** Talweg: High-Assurance Flash Translation Layer
** Static Geometry Signatures and Coordinate Proofs
*)

#define ATS_DYNLOADFLAG 0

sortdef nat = {a:int | a >= 0}
sortdef pos = {a:int | a > 0}

typedef channel_id (n:int) = [i:nat | i < n] int(i)
typedef die_id (n:int) = [i:nat | i < n] int(i)
typedef plane_id (n:int) = [i:nat | i < n] int(i)
typedef block_id (n:int) = [i:nat | i < n] int(i)
typedef page_offset (n:int) = [i:nat | i < n] int(i)

typedef lba_idx (n:int) = [i:nat | i < n] int(i)
typedef ppn_idx (n:int) = [i:nat | i < n] int(i)

(* Pure coordinate conversion functions *)
fun geom_ppn_to_block (ppn: uint, ppb: uint): uint = "mac#talweg_ats_ppn_to_block"
fun geom_ppn_to_offset (ppn: uint, ppb: uint): uint = "mac#talweg_ats_ppn_to_offset"
fun geom_coords_to_ppn (blk: uint, off: uint, ppb: uint): uint = "mac#talweg_ats_coords_to_ppn"

(* Static boundary check predicates with proof-carrying signatures *)
fun geom_lba_valid {m:int}{i:int} (max_lba: int(m), lba: int(i)): bool (i >= 0 && i < m) = "mac#talweg_ats_lba_valid"
fun geom_ppn_valid {p:int}{i:int} (total_pages: int(p), ppn: int(i)): bool (i >= 0 && i < p) = "mac#talweg_ats_ppn_valid"
fun geom_block_valid {b:int}{i:int} (total_blocks: int(b), blk: int(i)): bool (i >= 0 && i < b) = "mac#talweg_ats_block_valid"
fun geom_page_offset_valid {ppb:int}{i:int} (pages_per_block: int(ppb), off: int(i)): bool (i >= 0 && i < ppb) = "mac#talweg_ats_page_offset_valid"
