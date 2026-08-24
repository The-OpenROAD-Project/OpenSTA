# Examples

To read a design into OpenSTA use the `read_liberty` command to read
Liberty library files. Next, read hierarchical structural Verilog files
with the `read_verilog` command. The `link_design` command links the
Verilog to the Liberty timing cells. Any number of Liberty and Verilog
files can be read before linking the design.

Delays used for timing analysis are calculated using the Liberty timing
models. If no parasitics are read only the pin capacitances of the
timing models are used in delay calculation. Use the `read_spef`
command to read parasitics from an extractor, or `read_sdf` to use
delays calculated by an external delay calculator.

Timing constraints can be entered as Tcl commands or read using the
`read_sdc` command.

The units used by OpenSTA for all command arguments and reports are
taken from the first Liberty file that is read. Use the `set_cmd_units`
command to override the default units. Use the `report_units` command
to see the command units.

## Timing analysis using SDF

A sample command file that reads a library and a Verilog netlist and
reports timing checks is shown below.

```tcl
read_liberty example1_slow.lib
read_verilog example1.v
link_design top
read_sdf example1.sdf
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
report_checks
```

This example can be found in `examples/sdf_delays.tcl`.

## Timing analysis with multiple scenes

An example command script using three scenes and +/-10% min/max
derating is shown below.

```tcl
read_liberty nangate45_slow.lib.gz
read_liberty nangate45_typ.lib.gz
read_liberty nangate45_fast.lib.gz
read_verilog example1.v
link_design top
set_timing_derate -early 0.9
set_timing_derate -late 1.1
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}

define_scene ss -liberty nangate45_slow
define_scene tt -liberty nangate45_typ
define_scene ff -liberty nangate45_fast

# report all scenes
report_checks -path_delay min_max
# report typical scene
report_checks -scene tt
```

This example can be found in `examples/multi_corner.tcl`. Other examples
can be found in the `examples` directory.

## Timing analysis with multiple modes and scenes

OpenSTA supports multi-corner, multi-mode analysis. SDC constraints in
each mode describe an operating mode such as mission or scan. A scene
is a combination of a mode with Liberty libraries and SPEF parasitics.

A mode named "default" is initially created for SDC commands. It is
deleted when a mode is defined with `set_mode` or `read_sdc -mode`.
Similarly, a scene named "default" is initially created that is deleted
when `define_scene` is used to define a scene.

An example command script using two scenes and two modes is
shown below.

```tcl
read_liberty asap7_small_ff.lib.gz
read_liberty asap7_small_ss.lib.gz
read_verilog reg1_asap7.v
link_design top

read_sdc -mode mode1 mcmm2_mode1.sdc
read_sdc -mode mode2 mcmm2_mode2.sdc

read_spef -name reg1_ff reg1_asap7.spef
read_spef -name reg1_ss reg1_asap7_ss.spef

define_scene scene1 -mode mode1 -liberty asap7_small_ff -spef reg1_ff
define_scene scene2 -mode mode2 -liberty asap7_small_ss -spef reg1_ss

report_checks -scenes scene1
report_checks -scenes scene2
report_checks -group_path_count 4
```

This example can be found in `examples/mcmm3.tcl`.

In the example shown above the SDC for each mode is defined in a
separate file. Alternatively, the SDC can be defined in the command
file using the `set_mode` command between SDC command groups.

```tcl
set_mode mode1
create_clock -name m1_clk -period 1000 {clk1 clk2 clk3}
set_input_delay -clock m1_clk 100 {in1 in2}

set_mode mode2
create_clock -name m2_clk -period 500 {clk1 clk3}
set_output_delay -clock m2_clk 100 out
```

## Statistical timing analysis

OpenSTA also supports statistical timing analysis with Liberty Variation
Format (LVF) libraries. Statistical timing uses a probability
distribution to represent a delay or slew rather than a single number.

Normal and skew normal probability distributions are supported. SSTA is
enabled with the `sta_pocv_mode` variable.

```tcl
set sta_pocv_mode scalar|normal|skew_normal
```

- `scalar` mode is for non-SSTA analysis
- `normal` mode uses gaussian normal distributions
- `skew_normal` mode is for skew normal LVF moment based distributions

The target quantile of a delay probability distribution (confidence
level) is set with the `sta_pocv_quantile` variable.

```tcl
set sta_pocv_quantile <float>
```

The default value is 3 standard deviations, or sigma.

Use the `variation` field with the `report_checks` and
`report_check_types` commands to see distribution parameters in timing
reports.

A command file for analyzing a design with statistical timing is shown
below.

```tcl
read_liberty lvf_library.lib.gz
read_verilog design.v
link_design top
create_clock -period 50 clk
set_input_delay -clock clk 1 {in1 in2}
set sta_pocv_mode skew_normal
report_checks -fields {slew variation input_pin} -digits 3
```

The standard deviation for normal distributions is specified with the
following Liberty timing groups.

```
ocv_sigma_cell_rise
ocv_sigma_cell_fall
ocv_sigma_rise_transition
ocv_sigma_fall_transition
ocv_sigma_rise_constraint
ocv_sigma_fall_constraint
```

LVF skew normal distributions are specified with the Liberty groups
below.

```
ocv_std_dev_cell_rise
ocv_std_dev_cell_fall
ocv_mean_shift_cell_rise
ocv_mean_shift_cell_fall
ocv_skewness_cell_rise
ocv_skewness_cell_fall

ocv_std_dev_rise_transition
ocv_std_dev_fall_transition
ocv_skewness_rise_transition
ocv_skewness_fall_transition
ocv_mean_shift_rise_transition
ocv_mean_shift_fall_transition

ocv_std_dev_rise_constraint
ocv_std_dev_fall_constraint
ocv_skewness_rise_constraint
ocv_skewness_fall_constraint
ocv_mean_shift_rise_constraint
ocv_mean_shift_fall_constraint
```

## Power analysis

OpenSTA also supports static power analysis with the `report_power`
command. Probabilistic switching activities are propagated from the
input ports to determine switching activities for internal pins.

```tcl
read_liberty sky130hd_tt.lib
read_verilog gcd_sky130hd.v
link_design gcd
read_sdc gcd_sky130hd.sdc
read_spef gcd_sky130hd.spef
set_power_activity -input -activity 0.1
set_power_activity -input_port reset -activity 0
report_power
```

In this example the activity for all inputs is set to `0.1`, and then
the activity for the `reset` signal is set to zero because it does not
switch during steady state operation.

This example can be found in `examples/power.tcl`.

Gate level simulation results can be used to get a more accurate power
estimate. For example, the Icarus Verilog simulator can be used to run
the test bench `examples/gcd_tb.v` for the gcd design in the previous
example.

```tcl
iverilog -o gcd_tb gcd_tb.v
vvp gcd_tb
```

The test bench writes the VCD (Value Change Dump) file
`gcd_sky130hd.vcd` which can then be read with the `read_vcd` command.

```tcl
read_liberty sky130hd_tt.lib
read_verilog gcd_sky130hd.v
link_design gcd
read_sdc gcd_sky130hd.sdc
read_spef gcd_sky130hd.spef
read_vcd -scope gcd_tb/gcd1 gcd_sky130hd.vcd.gz
report_power
```

This example can be found in `examples/power_vcd.tcl`.

Note that in this simple example design, simulation-based activities
do not significantly change the results.
