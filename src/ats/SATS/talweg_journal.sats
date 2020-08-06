(*
** Talweg: High-Assurance Flash Translation Layer
** Write-Ahead Journal and Checkpointing Formal Signatures
*)

#define ATS_DYNLOADFLAG 0

staload "./talweg_geom.sats"

absvtype journal_engine_vtype = ptr

(* Create and destroy write-ahead journal with linear resource tracking *)
fun journal_engine_create (capacity_records: uint): journal_engine_vtype = "mac#talweg_ats_journal_engine_create"
fun journal_engine_destroy (j: journal_engine_vtype): void = "mac#talweg_ats_journal_engine_destroy"

(* Monotonic append operations returning sequence tokens *)
fun journal_append_write (j: !journal_engine_vtype, lba: uint, new_ppn: uint, old_ppn: uint): ullint = "mac#talweg_ats_journal_append_write"
fun journal_append_trim (j: !journal_engine_vtype, lba: uint, old_ppn: uint): ullint = "mac#talweg_ats_journal_append_trim"
fun journal_append_erase (j: !journal_engine_vtype, blk: uint, count: uint): ullint = "mac#talweg_ats_journal_append_erase"

(* Journal sequence queries *)
fun journal_get_sequence (j: !journal_engine_vtype): ullint = "mac#talweg_ats_journal_get_sequence"
fun journal_get_count (j: !journal_engine_vtype): uint = "mac#talweg_ats_journal_get_count"
fun journal_clear (j: !journal_engine_vtype): void = "mac#talweg_ats_journal_clear"
