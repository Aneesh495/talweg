(*
** Talweg: High-Assurance Flash Translation Layer
** Garbage Collection Victim Selection Formal Signatures
*)

#define ATS_DYNLOADFLAG 0

staload "./talweg_geom.sats"

absvtype gc_engine_vtype = ptr

(* Create and destroy GC engine with linear resource tracking *)
fun gc_engine_create (total_blocks: uint, ppb: uint, policy: int): gc_engine_vtype = "mac#talweg_ats_gc_engine_create"
fun gc_engine_destroy (gc: gc_engine_vtype): void = "mac#talweg_ats_gc_engine_destroy"

(* Block page state bookkeeping *)
fun gc_engine_invalidate_page (gc: !gc_engine_vtype, blk: uint): void = "mac#talweg_ats_gc_engine_invalidate_page"
fun gc_engine_validate_page (gc: !gc_engine_vtype, blk: uint): void = "mac#talweg_ats_gc_engine_validate_page"
fun gc_engine_get_invalid_count (gc: !gc_engine_vtype, blk: uint): uint = "mac#talweg_ats_gc_engine_get_invalid_count"
fun gc_engine_get_valid_count (gc: !gc_engine_vtype, blk: uint): uint = "mac#talweg_ats_gc_engine_get_valid_count"

(* Victim selection and reclamation *)
fun gc_engine_select_victim (gc: !gc_engine_vtype): uint = "mac#talweg_ats_gc_engine_select_victim"
fun gc_engine_reset_block (gc: !gc_engine_vtype, blk: uint): void = "mac#talweg_ats_gc_engine_reset_block"
fun gc_engine_set_policy (gc: !gc_engine_vtype, policy: int): void = "mac#talweg_ats_gc_engine_set_policy"
