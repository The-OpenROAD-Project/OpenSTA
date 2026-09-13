# DFFHQx4 and DHLx1 have the same ports, the same Q function and the same
# clock/data expressions.  Only the ff/latch group differs, so they must not
# compare equivalent.
#
# find_equiv_cells on a cell with no equivalents returns an empty list rather
# than crashing.

read_liberty asap7_seq.lib.gz

set lib asap7sc7p5t_SEQ_RVT_TT_ccs_220123
set ff [get_lib_cell $lib/DFFHQx4_ASAP7_75t_R]
set latch [get_lib_cell $lib/DHLx1_ASAP7_75t_R]

# The ports match, which is what puts the two cells in the same hash bucket.
puts "equiv_cell_ports ff latch: [sta::equiv_cell_ports $ff $latch]"
puts "equiv_cells ff latch: [sta::equiv_cells $ff $latch]"

sta::make_equiv_cells [lindex [get_libs $lib] 0]

# DFFHQx4 is the only x4 drive ff, so it has no equivalents.
puts "find_equiv_cells ff: [llength [sta::find_equiv_cells $ff]]"

# DFFHQNx1/x2/x3 are equivalent to each other, so the ff group must survive.
set ffn [get_lib_cell $lib/DFFHQNx1_ASAP7_75t_R]
puts "find_equiv_cells DFFHQNx1: [llength [sta::find_equiv_cells $ffn]]"
