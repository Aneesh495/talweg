(*
** Talweg: High-Assurance Flash Translation Layer
** Bad Block Table and Defect Retirement Formal Signatures
*)

#define ATS_DYNLOADFLAG 0

staload "./talweg_geom.sats"

absvtype bbt_engine_vtype = ptr

(* Create and destroy bad block table with linear resource management *)
fun bbt_engine_create (total_blocks: uint, reserve_blocks: uint): bbt_engine_vtype = "mac#talweg_ats_bbt_engine_create"
fun bbt_engine_destroy (bbt: bbt_engine_vtype): void = "mac#talweg_ats_bbt_engine_destroy"

(* Defect marking and lookup operations *)
fun bbt_mark_bad (bbt: !bbt_engine_vtype, blk: uint): int = "mac#talweg_ats_bbt_mark_bad"
fun bbt_is_bad (bbt: !bbt_engine_vtype, blk: uint): bool = "mac#talweg_ats_bbt_is_bad"
fun bbt_get_remap (bbt: !bbt_engine_vtype, blk: uint): uint = "mac#talweg_ats_bbt_get_remap"

(* Spare block pool metrics *)
fun bbt_get_bad_count (bbt: !bbt_engine_vtype): uint = "mac#talweg_ats_bbt_get_bad_count"
fun bbt_get_remaining_spares (bbt: !bbt_engine_vtype): uint = "mac#talweg_ats_bbt_get_remaining_spares"
