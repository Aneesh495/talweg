(*
** Talweg: High-Assurance Flash Translation Layer
** Unified FTL Controller Core Formal Signatures
*)

#define ATS_DYNLOADFLAG 0

staload "./talweg_geom.sats"
staload "./talweg_lba_map.sats"
staload "./talweg_alloc.sats"
staload "./talweg_wear.sats"
staload "./talweg_gc.sats"
staload "./talweg_journal.sats"
staload "./talweg_bad_block.sats"
staload "./talweg_cache.sats"

absvtype talweg_ftl_vtype = ptr

(* Lifecycle management *)
fun ftl_create_instance (total_blocks: uint, ppb: uint, page_sz: uint, max_lba: uint, gc_policy: int): ptr = "mac#talweg_ats_ftl_create"
fun ftl_destroy_instance (ftl: ptr): void = "mac#talweg_ats_ftl_destroy"

(* Core storage transactions *)
fun ftl_read_lba (ftl: ptr, lba: uint, out_buf: ptr): int = "mac#talweg_ats_ftl_read"
fun ftl_write_lba (ftl: ptr, lba: uint, in_buf: ptr): int = "mac#talweg_ats_ftl_write"
fun ftl_trim_lba (ftl: ptr, lba: uint): int = "mac#talweg_ats_ftl_trim"
fun ftl_flush_all (ftl: ptr): int = "mac#talweg_ats_ftl_flush"
fun ftl_step_gc (ftl: ptr): int = "mac#talweg_ats_ftl_step_gc"

(* Query metrics *)
fun ftl_get_total_user_writes (ftl: ptr): ullint = "mac#talweg_ats_ftl_get_user_writes"
fun ftl_get_total_flash_writes (ftl: ptr): ullint = "mac#talweg_ats_ftl_get_flash_writes"
fun ftl_get_total_gc_cycles (ftl: ptr): ullint = "mac#talweg_ats_ftl_get_gc_cycles"
fun ftl_get_waf_mille (ftl: ptr): uint = "mac#talweg_ats_ftl_get_waf_mille"
