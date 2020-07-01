(*
** Talweg: High-Assurance Flash Translation Layer
** Dynamic Geometry Implementation and Coordinate Transformations
*)

#define ATS_DYNLOADFLAG 0

#include "share/atspre_staload.hats"
staload "./../SATS/talweg_geom.sats"

implement geom_ppn_to_block (ppn, ppb) =
  if ppb > 0U then ppn / ppb else 0U

implement geom_ppn_to_offset (ppn, ppb) =
  if ppb > 0U then ppn mod ppb else 0U

implement geom_coords_to_ppn (blk, off, ppb) =
  blk * ppb + off

implement geom_lba_valid {m}{i} (max_lba, lba) =
  if lba >= 0 then
    (if lba < max_lba then true else false)
  else false

implement geom_ppn_valid {p}{i} (total_pages, ppn) =
  if ppn >= 0 then
    (if ppn < total_pages then true else false)
  else false

implement geom_block_valid {b}{i} (total_blocks, blk) =
  if blk >= 0 then
    (if blk < total_blocks then true else false)
  else false

implement geom_page_offset_valid {ppb}{i} (pages_per_block, off) =
  if off >= 0 then
    (if off < pages_per_block then true else false)
  else false
