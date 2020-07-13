(*
** Talweg: High-Assurance Flash Translation Layer
** Linear Block Allocator and State Machine Signatures
*)

#define ATS_DYNLOADFLAG 0

staload "./talweg_geom.sats"

absvtype block_allocator_vtype = ptr

(* Create and destroy block allocator with linear ownership *)
fun block_alloc_create (total_blocks: uint, ppb: uint): block_allocator_vtype = "mac#talweg_ats_block_alloc_create"
fun block_alloc_destroy (alloc: block_allocator_vtype): void = "mac#talweg_ats_block_alloc_destroy"

(* Query counts and capacity *)
fun block_alloc_get_total (alloc: !block_allocator_vtype): uint = "mac#talweg_ats_block_alloc_get_total"
fun block_alloc_get_free_count (alloc: !block_allocator_vtype): uint = "mac#talweg_ats_block_alloc_get_free_count"
fun block_alloc_get_active_block (alloc: !block_allocator_vtype): uint = "mac#talweg_ats_block_alloc_get_active_block"
fun block_alloc_get_active_offset (alloc: !block_allocator_vtype): uint = "mac#talweg_ats_block_alloc_get_active_offset"

(* Allocation operations *)
fun block_alloc_next_page (alloc: !block_allocator_vtype, out_ppn: &uint? >> uint): int = "mac#talweg_ats_block_alloc_next_page"
fun block_alloc_acquire_free (alloc: !block_allocator_vtype): uint = "mac#talweg_ats_block_alloc_acquire_free"
fun block_alloc_release_erased (alloc: !block_allocator_vtype, blk: uint): void = "mac#talweg_ats_block_alloc_release_erased"
fun block_alloc_mark_bad (alloc: !block_allocator_vtype, blk: uint): void = "mac#talweg_ats_block_alloc_mark_bad"
