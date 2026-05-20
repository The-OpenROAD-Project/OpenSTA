# Sta::makeConcreteParasitics map-overwrite leak repro.
# Run under -fsanitize=address to verify no leak after the fix.
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
# 1st read_spef
read_spef -min reg1_asap7.spef
# 2nd read_spef
read_spef -min reg1_asap7.spef
