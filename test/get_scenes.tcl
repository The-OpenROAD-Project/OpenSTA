# get_scenes / get_modes return objects
read_liberty ../examples/asap7_small_ff.lib.gz
read_liberty ../examples/asap7_small_ss.lib.gz
read_verilog ../examples/reg1_asap7.v
link_design top

read_sdc -mode mode1 ../examples/mcmm2_mode1.sdc
read_sdc -mode mode2 ../examples/mcmm2_mode2.sdc

read_spef -name reg1_ff ../examples/reg1_asap7.spef
read_spef -name reg1_ss ../examples/reg1_asap7_ss.spef

define_scene scene1 -mode mode1 -liberty asap7_small_ff -spef reg1_ff
define_scene scene2 -mode mode2 -liberty asap7_small_ss -spef reg1_ss

puts {[get_scenes *]}
report_object_names [get_scenes *]

puts {[get_modes *]}
report_object_names [get_modes *]

puts {[get_scenes -filter {name == scene1}]}
report_object_names [get_scenes -filter {name == scene1}]

puts {[get_scenes -filter {name =~ scene*}]}
report_object_names [get_scenes -filter {name =~ scene*}]

puts {[get_scenes -filter {name != scene1}]}
report_object_names [get_scenes -filter {name != scene1}]

puts {[get_modes -filter {name == mode2}]}
report_object_names [get_modes -filter {name == mode2}]

puts {[get_modes -filter {name =~ mode*}]}
report_object_names [get_modes -filter {name =~ mode*}]

# User-defined bool property. scene1 set active, scene2 left unset.
define_property -object_type scene -type bool active
set_property [get_scenes scene1] active true

puts {[get_property scene1 active]}
puts [get_property [get_scenes scene1] active]
puts {[get_property scene2 active] (unset)}
puts "<[get_property [get_scenes scene2] active]>"

puts {[get_scenes -filter {active == true}]}
report_object_names [get_scenes -filter {active == true}]

# Unset does not match false; only an explicitly set value does.
puts {[get_scenes -filter {active == false}] (scene2 unset)}
report_object_names [get_scenes -filter {active == false}]

puts {[get_scenes -filter {active}]}
report_object_names [get_scenes -filter {active}]

puts {[get_scenes -filter {!active}]}
report_object_names [get_scenes -filter {!active}]

set_property [get_scenes scene2] active false
puts {[get_property scene2 active] (set false)}
puts [get_property [get_scenes scene2] active]

puts {[get_scenes -filter {active == false}] (scene2 set false)}
report_object_names [get_scenes -filter {active == false}]
