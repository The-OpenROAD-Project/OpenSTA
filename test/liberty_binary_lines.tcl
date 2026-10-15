# Messages about a library read from a .blib cite the line numbers of the
# text .lib it was written from.
source helpers.tcl

read_liberty liberty_arcs_one2one_1.lib

make_result_file liberty_arcs_one2one_1.blib
# Relative so the messages below are reproducible.
set blib_file results/liberty_arcs_one2one_1.blib
write_liberty_binary liberty_arcs_one2one_1.lib $blib_file
read_liberty $blib_file
