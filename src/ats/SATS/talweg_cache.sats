(*
** Talweg: High-Assurance Flash Translation Layer
** Linear-Pinned Page Cache Formal Signatures
*)

#define ATS_DYNLOADFLAG 0

staload "./talweg_geom.sats"

absvtype cache_pool_vtype = ptr

(* Create and destroy page cache pool with linear resource tracking *)
fun cache_pool_create (capacity_pages: uint, page_size: uint): cache_pool_vtype = "mac#talweg_ats_cache_pool_create"
fun cache_pool_destroy (c: cache_pool_vtype): void = "mac#talweg_ats_cache_pool_destroy"

(* Lookup, pin, and unpin operations *)
fun cache_pool_lookup (c: !cache_pool_vtype, lba: uint): int = "mac#talweg_ats_cache_pool_lookup"
fun cache_pool_pin (c: !cache_pool_vtype, lba: uint, out_buf: &ptr? >> ptr): int = "mac#talweg_ats_cache_pool_pin"
fun cache_pool_unpin (c: !cache_pool_vtype, lba: uint, is_dirty: bool): int = "mac#talweg_ats_cache_pool_unpin"

(* Dirty frame flush and hit/miss statistics *)
fun cache_pool_get_dirty_count (c: !cache_pool_vtype): uint = "mac#talweg_ats_cache_pool_get_dirty_count"
fun cache_pool_get_hit_count (c: !cache_pool_vtype): ullint = "mac#talweg_ats_cache_pool_get_hit_count"
fun cache_pool_get_miss_count (c: !cache_pool_vtype): ullint = "mac#talweg_ats_cache_pool_get_miss_count"
fun cache_pool_invalidate (c: !cache_pool_vtype, lba: uint): void = "mac#talweg_ats_cache_pool_invalidate"
