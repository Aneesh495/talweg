(*
** Talweg: High-Assurance Flash Translation Layer
** Wear-Leveling Heap and Delta Invariant Formal Signatures
*)

#define ATS_DYNLOADFLAG 0

staload "./talweg_geom.sats"

absvtype wear_tracker_vtype = ptr

(* Create and destroy wear tracker with linear resource management *)
fun wear_tracker_create (total_blocks: uint, delta_thresh: uint): wear_tracker_vtype = "mac#talweg_ats_wear_tracker_create"
fun wear_tracker_destroy (wt: wear_tracker_vtype): void = "mac#talweg_ats_wear_tracker_destroy"

(* Erase event recording and metric queries *)
fun wear_tracker_record_erase (wt: !wear_tracker_vtype, blk: uint): uint = "mac#talweg_ats_wear_tracker_record_erase"
fun wear_tracker_get_erase_count (wt: !wear_tracker_vtype, blk: uint): uint = "mac#talweg_ats_wear_tracker_get_erase_count"
fun wear_tracker_get_min_erase (wt: !wear_tracker_vtype): uint = "mac#talweg_ats_wear_tracker_get_min_erase"
fun wear_tracker_get_max_erase (wt: !wear_tracker_vtype): uint = "mac#talweg_ats_wear_tracker_get_max_erase"
fun wear_tracker_get_wear_delta (wt: !wear_tracker_vtype): uint = "mac#talweg_ats_wear_tracker_get_wear_delta"

(* Wear leveling condition checks and candidate selection *)
fun wear_tracker_needs_static_rebalance (wt: !wear_tracker_vtype): bool = "mac#talweg_ats_wear_tracker_needs_static_rebalance"
fun wear_tracker_find_cold_candidate (wt: !wear_tracker_vtype): uint = "mac#talweg_ats_wear_tracker_find_cold_candidate"
