(*
** Talweg: High-Assurance Flash Translation Layer
** Logical-to-Physical (L2P) Mapping Table Formal Signatures
*)

#define ATS_DYNLOADFLAG 0

staload "./talweg_geom.sats"

absvtype lba_map_table_vtype = ptr

(* Create and destroy mapping table with linear ownership tracking *)
fun lba_map_create (capacity: uint): lba_map_table_vtype = "mac#talweg_ats_lba_map_create"
fun lba_map_destroy (tbl: lba_map_table_vtype): void = "mac#talweg_ats_lba_map_destroy"

(* Pure queries on mapping table *)
fun lba_map_lookup (tbl: !lba_map_table_vtype, lba: uint): uint = "mac#talweg_ats_lba_map_lookup"
fun lba_map_is_mapped (tbl: !lba_map_table_vtype, lba: uint): bool = "mac#talweg_ats_lba_map_is_mapped"
fun lba_map_count_mapped (tbl: !lba_map_table_vtype): uint = "mac#talweg_ats_lba_map_count_mapped"
fun lba_map_get_capacity (tbl: !lba_map_table_vtype): uint = "mac#talweg_ats_lba_map_get_capacity"

(* Atomic update and unmap returning previous physical page binding *)
fun lba_map_update (tbl: !lba_map_table_vtype, lba: uint, new_ppn: uint): uint = "mac#talweg_ats_lba_map_update"
fun lba_map_unmap (tbl: !lba_map_table_vtype, lba: uint): uint = "mac#talweg_ats_lba_map_unmap"
