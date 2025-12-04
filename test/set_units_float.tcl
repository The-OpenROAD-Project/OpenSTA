# set_units with floating point numbers

# start by setting the cmd units
set_cmd_units -capacitance 1000.0fF
set_cmd_units -resistance 1.0kohm
set_cmd_units -time 1.0ns
set_cmd_units -voltage 1.0v
set_cmd_units -current 1.0A
set_cmd_units -distance 1.0um

# then check the units
set_units -capacitance 1.0pF
set_units -resistance 1.0kohm
set_units -time 1.0ns
set_units -voltage 1.0v
set_units -current 1.0A
set_units -power 1.0W
set_units -distance 1.0um

# finally report the units
report_units
