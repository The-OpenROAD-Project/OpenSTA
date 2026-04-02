# Test for Levelize::levelizedDrvrVertices() API.
# Verifies:
#   1. Returns driver vertices in non-decreasing level order
#   2. All returned vertices are drivers
source helpers.tcl
read_liberty asap7/asap7sc7p5t_INVBUF_RVT_TT_nldm_220122.lib.gz
read_liberty asap7/asap7sc7p5t_INVBUF_LVT_TT_nldm_220122.lib.gz
read_liberty asap7/asap7sc7p5t_INVBUF_SLVT_TT_nldm_220122.lib.gz
read_verilog gcd_asap7.v
link_design gcd
create_clock -name clk -period 500 {clk}

# Report first 100 driver vertices in level order and total count
sta::report_levelized_drvr_vertices 100
