# User-defined properties on pin/net/instance/clock object types.
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk1 -period 10 {clk1}

# pin: string property, set on one pin, left unset on another.
define_property -object_type pin -type string owner
set_property [get_pins u1/Z] owner alice
puts {[get_property u1/Z owner]}
puts [get_property [get_pins u1/Z] owner]
puts {[get_property u2/ZN owner] (unset)}
puts "<[get_property [get_pins u2/ZN] owner]>"

# net: float property.
define_property -object_type net -type float weight
set_property [get_nets r1q] weight 3.5
puts {[get_property r1q weight]}
puts [get_property [get_nets r1q] weight]
puts {[get_property u1z weight] (unset)}
puts "<[get_property [get_nets u1z] weight]>"

# instance: bool property.
define_property -object_type instance -type bool crit
set_property [get_cells u2] crit true
puts {[get_property u2 crit]}
puts [get_property [get_cells u2] crit]
puts {[get_property u1 crit] (unset)}
puts "<[get_property [get_cells u1] crit]>"

# clock: string property.
define_property -object_type clock -type string grp
set_property [get_clocks clk1] grp main
puts {[get_property clk1 grp]}
puts [get_property [get_clocks clk1] grp]

# -filter skips objects the property was never set on (no error).
puts {[get_pins -filter {owner == alice} *]}
report_object_names [get_pins -filter {owner == alice} *]

# An undefined property still errors.
if {[catch {get_property [get_pins u1/Z] no_such_prop} msg]} {
  puts $msg
}
