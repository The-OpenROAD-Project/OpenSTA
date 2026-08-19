# Commands

This page is generated from the live command registry.
Do not edit it by hand; rebuild `sta` to regenerate.

Use `help <command>` in the Tcl interpreter for the same text.

## all_clocks

<pre><code>all_clocks</code></pre>

The `all_clocks` command returns a list of all clocks that have been defined.

## all_inputs

<pre><code>all_inputs
    [<a href="#opt-all_inputs-no_clocks">-no_clocks</a>]</code></pre>

The `all_inputs` command returns a list of all input and bidirect ports of the current design.

### Options

`-no_clocks` {: #opt-all_inputs-no_clocks }
: Exclude inputs defined as clock sources.

## all_outputs

<pre><code>all_outputs</code></pre>

The `all_outputs` command returns a list of all output and bidirect ports of the design.

## all_registers

<pre><code>all_registers
    [<a href="#opt-all_registers-clock">-clock</a> clocks]
    [<a href="#opt-all_registers-rise_clock">-rise_clock</a> clocks]
    [<a href="#opt-all_registers-fall_clock">-fall_clock</a> clocks]
    [<a href="#opt-all_registers-cells">-cells</a>]
    [<a href="#opt-all_registers-data_pins">-data_pins</a>]
    [<a href="#opt-all_registers-clock_pins">-clock_pins</a>]
    [<a href="#opt-all_registers-async_pins">-async_pins</a>]
    [<a href="#opt-all_registers-output_pins">-output_pins</a>]
    [<a href="#opt-all_registers-level_sensitive">-level_sensitive</a>]
    [<a href="#opt-all_registers-edge_triggered">-edge_triggered</a>]</code></pre>

The `all_registers` command returns a list of  register instances or register pins in the design. Options allow the list of registers to be restricted in various ways. The `-clock` keyword restrcts the registers to those that are clocked by a set of clocks. The `-cells` option returns the list of registers or latches (the default). The `-data_pins`, `-clock_pins`, `-async_pins` and `-output_pins` options cause `all_registers` to return a list of register pins rather than instances.

### Options

`-clock` {: #opt-all_registers-clock }
: `clock_names`: A list of clock names. Only registers clocked by these clocks are returned.

`-rise_clock` {: #opt-all_registers-rise_clock }
: Only registers clocked by the rising edge of these clocks are returned.

`-fall_clock` {: #opt-all_registers-fall_clock }
: Only registers clocked by the falling edge of these clocks are returned.

`-cells` {: #opt-all_registers-cells }
: Return a list of register instances.

`-data_pins` {: #opt-all_registers-data_pins }
: Return the register data pins.

`-clock_pins` {: #opt-all_registers-clock_pins }
: Return the register clock pins.

`-async_pins` {: #opt-all_registers-async_pins }
: Return the register set/clear pins.

`-output_pins` {: #opt-all_registers-output_pins }
: Return the register output pins.

`-level_sensitive` {: #opt-all_registers-level_sensitive }
: Return level-sensitive latches.

`-edge_triggered` {: #opt-all_registers-edge_triggered }
: Return edge-triggered registers.

## check_setup

<pre><code>check_setup
    [<a href="#opt-check_setup-verbose">-verbose</a>]
    [<a href="#opt-check_setup-no_input_delay">-no_input_delay</a>]
    [<a href="#opt-check_setup-no_output_delay">-no_output_delay</a>]
    [<a href="#opt-check_setup-multiple_clock">-multiple_clock</a>]
    [<a href="#opt-check_setup-no_clock">-no_clock</a>]
    [<a href="#opt-check_setup-unconstrained_endpoints">-unconstrained_endpoints</a>]
    [<a href="#opt-check_setup-loops">-loops</a>]
    [<a href="#opt-check_setup-generated_clocks">-generated_clocks</a>]
    [&gt; filename]
    [&gt;&gt; filename]</code></pre>

The `check_setup` command performs sanity checks on the design. Individual checks can be performed with the keywords. If no check keywords are specified all checks are performed. Checks that fail are reported as warnings. If no checks fail nothing is reported. The command returns 1 if there are no warnings for use in scripts.

### Options

`-verbose` {: #opt-check_setup-verbose }
: Show offending objects rather than just error counts.

`-no_input_delay` {: #opt-check_setup-no_input_delay }
: Check for inputs that do not have a `set_input_delay` command.

`-no_output_delay` {: #opt-check_setup-no_output_delay }
: Check for outputs that do not have a `set_output_delay` command.

`-multiple_clock` {: #opt-check_setup-multiple_clock }
: Check register/latch clock pins for multiple clocks.

`-no_clock` {: #opt-check_setup-no_clock }
: Check register/latch clock pins for a clock.

`-unconstrained_endpoints` {: #opt-check_setup-unconstrained_endpoints }
: Check path endpoints for timing constraints (timing check or `set_output_delay`).

`-loops` {: #opt-check_setup-loops }
: Check for combinational logic loops.

`-generated_clocks` {: #opt-check_setup-generated_clocks }
: Check that generated clock source pins have been defined as clocks.

## connect_pin

<pre><code>connect_pin
    net
    pin</code></pre>

The `connect_pin` command connects a port or instance pin to a net.

## create_clock

<pre><code>create_clock
    [<a href="#opt-create_clock-name">-name</a> name]
    [<a href="#opt-create_clock-period">-period</a> period]
    [<a href="#opt-create_clock-waveform">-waveform</a> waveform]
    [<a href="#opt-create_clock-add">-add</a>]
    [<a href="#opt-create_clock-comment">-comment</a> comment]
    [pins]</code></pre>

The `create_clock` command defines the waveform of a clock used by the design.

If no pin_list is specified the clock is virtual. A virtual clock can be refered to by name in input arrival and departure time commands but is not attached to any pins in the design.

If no clock name is specified the name of the first pin is used as the clock name.

If a wavform is not specified the clock rises at zero and falls at half the clock period. The waveform is a list with time the clock rises as the first element and the time it falls as the second element.

If a clock is already defined on a pin the clock is redefined using the new clock parameters. If multiple clocks drive the same pin, use the `-add` option to prevent the existing definition from being overwritten.

The following command creates a clock with a period of 10 time units that rises at time 0 and falls at 5 time units on the pin named clk1.

```
create_clock -period 10 clk1
```

The following command creates a clock with a period of 10 time units that is high at time zero, falls at time 2 and rises at time 8. The clock drives three pins named clk1, clk2, and clk3.

```
create_clock -period 10 -waveform {8 2} -name clk {clk1 clk2 clk3}
```

### Options

`-name` {: #opt-create_clock-name }
: `clock_name`: The name of the clock.

`-period` {: #opt-create_clock-period }
: `period`: The clock period.

`-waveform` {: #opt-create_clock-waveform }
: `edge_list`: A list of edge rise and fall time.

`-add` {: #opt-create_clock-add }
: Add this clock to the clocks on pin_list.

`-comment` {: #opt-create_clock-comment }
: Comment string saved with the constraint.

## create_generated_clock

<pre><code>create_generated_clock
    [<a href="#opt-create_generated_clock-name">-name</a> clock_name]
    <a href="#opt-create_generated_clock-source">-source</a>
    master_pin
    [<a href="#opt-create_generated_clock-master_clock">-master_clock</a> clock]
    [<a href="#opt-create_generated_clock-divide_by">-divide_by</a> divisor | <a href="#opt-create_generated_clock-multiply_by">-multiply_by</a> multiplier]
    [<a href="#opt-create_generated_clock-duty_cycle">-duty_cycle</a> duty_cycle]
    [<a href="#opt-create_generated_clock-invert">-invert</a>]
    [<a href="#opt-create_generated_clock-edges">-edges</a> edge_list]
    [<a href="#opt-create_generated_clock-edge_shift">-edge_shift</a> edge_shift_list]
    [<a href="#opt-create_generated_clock-combinational">-combinational</a>]
    [<a href="#opt-create_generated_clock-add">-add</a>]
    [<a href="#opt-create_generated_clock-comment">-comment</a> comment]
    port_pin_list</code></pre>

The `create_generated_clock` command is used to generate a clock from an existing clock definition. It is used to model clock generation circuits such as clock dividers and phase locked loops.

The `-divide_by`, `-multiply_by` and `-edges` arguments are mutually exclusive.

The `-multiply_by` option is used to generate a higher frequency clock from the source clock. The period of the generated clock is divided by multiplier. The clock multiplier must be a positive integer. If a duty cycle is specified the generated clock rises at zero and falls at period * duty_cycle / 100. If no duty cycle is specified the source clock edge times are divided by multiplier.

The `-divide_by` option is used to generate a lower frequency clock from the source clock. The clock divisor must be a positive integer. If the clock divisor is a power of two the source clock period is multiplied by divisor, the clock rise time is the same as the source clock, and the clock fall edge is one half period later. If the clock divisor is not a power of two the source clock waveform edge times are multiplied by divisor.

The `-edges` option forms the generated clock waveform by selecting edges from the source clock waveform.

If the `-invert` option is specified the waveform derived above is inverted.

If a clock is already defined on a pin the clock is redefined using the new clock parameters. If multiple clocks drive the same pin, use the `-add` option to prevent the existing definition from being overwritten.

In the example show below generates a clock named gclk1 on register output pin r1/Q by dividing it by four.

```
create_clock -period 10 -waveform {1 8} clk1
create_generated_clock -name gclk1 -source clk1 -divide_by 4 r1/Q
```

The generated clock has a period of 40, rises at time 1 and falls at time 21.

In the example shown below the duty cycle is used to define the derived clock waveform.

```
create_generated_clock -name gclk1 -source clk1 -duty_cycle 50  -multiply_by 2 r1/Q
```

The generated clock has a period of 5, rises at time .5 and falls at time 3.

In the example shown below the first, third and fifth source clock edges are used to define the derived clock waveform.

```
create_generated_clock -name gclk1 -source clk1 -edges {1 3 5} r1/Q
```

The generated clock has a period of 20, rises at time 1 and falls at time 11.

### Options

`-name` {: #opt-create_generated_clock-name }
: `clock_name`: The name of the generated clock.

`-source` {: #opt-create_generated_clock-source }
: `master_pin`: A pin or port in the fanout of the master clock that is the source of the generated clock.

`-master_clock` {: #opt-create_generated_clock-master_clock }
: `master_clock`: Use `-master_clock` to specify which source clock to use when multiple clocks are present on master_pin.

`-divide_by` {: #opt-create_generated_clock-divide_by }
: `divisor`: Divide the master clock period by divisor.

`-multiply_by` {: #opt-create_generated_clock-multiply_by }
: `multiplier`: Multiply the master clock period by multiplier.

`-duty_cycle` {: #opt-create_generated_clock-duty_cycle }
: `duty_cycle`: The percent of the period that the generated clock is high (between 0 and 100).

`-invert` {: #opt-create_generated_clock-invert }
: Invert the master clock.

`-edges` {: #opt-create_generated_clock-edges }
: `edge_list`: List of master clock edges to use in the generated clock. Edges are numbered from 1. edge_list must be 3 edges long.

`-edge_shift` {: #opt-create_generated_clock-edge_shift }
: `shift_list`: Not supported.

`-combinational` {: #opt-create_generated_clock-combinational }
: The generated clock is combinational, equivalent to `-divide_by 1`.

`-add` {: #opt-create_generated_clock-add }
: Add this clock to the existing clocks on pin_list.

`-comment` {: #opt-create_generated_clock-comment }
: Comment string saved with the constraint.

## create_voltage_area

<pre><code>create_voltage_area
    [<a href="#opt-create_voltage_area-name">-name</a> name]
    [<a href="#opt-create_voltage_area-coordinate">-coordinate</a> coordinates]
    [<a href="#opt-create_voltage_area-guard_band_x">-guard_band_x</a> guard_x]
    [<a href="#opt-create_voltage_area-guard_band_y">-guard_band_y</a> guard_y]
    cells</code></pre>

This command is parsed and ignored by timing analysis.

### Options

`-name` {: #opt-create_voltage_area-name }
: Voltage area name. Ignored.

`-coordinate` {: #opt-create_voltage_area-coordinate }
: Voltage area coordinates. Ignored.

`-guard_band_x` {: #opt-create_voltage_area-guard_band_x }
: X guard band. Ignored.

`-guard_band_y` {: #opt-create_voltage_area-guard_band_y }
: Y guard band. Ignored.

## current_design

<pre><code>current_design
    [design]</code></pre>

Set or report the current design. OpenSTA only supports one design.

## current_instance

<pre><code>current_instance
    [instance]</code></pre>

Set or report the current instance used for relative name lookup.

## define_corners

<pre><code>define_corners
    corner1
    [corner2]
    ...</code></pre>

The `define_corners` command is deprecated. Use `define_scene` instead. It is supported for compatibility with older scripts that define analysis corners before `read_liberty`, but should not be used with MCMM flows.

## define_property

<pre><code>define_property
    <a href="#opt-define_property-object_type">-object_type</a>
    scene|mode|library|liberty_library|cell|liberty_cell|port|liberty_port|instance|pin|net|clock
    <a href="#opt-define_property-type">-type</a>
    bool|float|string
    property</code></pre>

The `define_property` command defines a user property that can be set with `set_property` and read with `get_property`. User properties can also be used in `-filter` expressions.

### Options

`-object_type` {: #opt-define_property-object_type }
: Object type the property applies to.

`-type` {: #opt-define_property-type }
: - `bool`: Boolean value.
  - `float`: Floating point value.
  - `string`: String value.

## define_scene

<pre><code>define_scene
    name
    [<a href="#opt-define_scene-mode">-mode</a> mode_name]
    [<a href="#opt-define_scene-liberty">-liberty</a> liberty_files | <a href="#opt-define_scene-liberty_min">-liberty_min</a> liberty_min_files <a href="#opt-define_scene-liberty_max">-liberty_max</a> liberty_max_files]
    [<a href="#opt-define_scene-spef">-spef</a> spef_file | <a href="#opt-define_scene-spef_min">-spef_min</a> spef_min_file <a href="#opt-define_scene-spef_max">-spef_max</a> spef_max_file]</code></pre>

The `define_scene` command defines a scene for a mode (SDC), liberty files and spef parasitics. Define scenes after reading Liberty libraries and SPEF parasitics.

Liberty files are specified with the name of the Liberty library or the filename of the Liberty file. If a filename is used, it must be the same as the filename used to read the library with `read_liberty`.

Use `get_scenes` to find defined scenes.

### Options

`-mode` {: #opt-define_scene-mode }
: The SDC mode to use. Defaults to the current mode.

`-liberty` {: #opt-define_scene-liberty }
: Liberty library name or filename used with `read_liberty`.

`-liberty_min` {: #opt-define_scene-liberty_min }
: Min-delay Liberty library name or filename.

`-liberty_max` {: #opt-define_scene-liberty_max }
: Max-delay Liberty library name or filename.

`-spef` {: #opt-define_scene-spef }
: SPEF parasitics name from `read_spef -name`.

`-spef_min` {: #opt-define_scene-spef_min }
: Min-delay SPEF parasitics name.

`-spef_max` {: #opt-define_scene-spef_max }
: Max-delay SPEF parasitics name.

## delete_clock

<pre><code>delete_clock
    [<a href="#opt-delete_clock-all">-all</a>]
    clocks</code></pre>

Delete clocks.

### Options

`-all` {: #opt-delete_clock-all }
: Delete all clocks.

## delete_from_list

<pre><code>delete_from_list
    list
    delete</code></pre>

Remove objects from a list.

## delete_generated_clock

<pre><code>delete_generated_clock
    [<a href="#opt-delete_generated_clock-all">-all</a>]
    clocks</code></pre>

Delete generated clocks.

### Options

`-all` {: #opt-delete_generated_clock-all }
: Delete all generated clocks.

## delete_instance

<pre><code>delete_instance
    inst</code></pre>

The network editing command `delete_instance` removes an instance from the design.

## delete_net

<pre><code>delete_net
    net</code></pre>

The network editing command `delete_net` removes a net from the design.

## disconnect_pin

<pre><code>disconnect_pin
    net
    <a href="#opt-disconnect_pin-all">-all</a>|pin</code></pre>

Disconnects a port or pin from a net. Parasitics connected to the pin are deleted.

### Options

`-all` {: #opt-disconnect_pin-all }
: Disconnect all pins from the net.

## elapsed_run_time

<pre><code>elapsed_run_time</code></pre>

Returns the total clock run time in seconds as a float.

## find_timing_paths

<pre><code>find_timing_paths
    [<a href="#opt-find_timing_paths-from">-from</a> from_list|<a href="#opt-find_timing_paths-rise_from">-rise_from</a> from_list|<a href="#opt-find_timing_paths-fall_from">-fall_from</a> from_list]
    [<a href="#opt-find_timing_paths-through">-through</a> through_list|<a href="#opt-find_timing_paths-rise_through">-rise_through</a> through_list|<a href="#opt-find_timing_paths-fall_through">-fall_through</a> through_list]
    [<a href="#opt-find_timing_paths-to">-to</a> to_list|<a href="#opt-find_timing_paths-rise_to">-rise_to</a> to_list|<a href="#opt-find_timing_paths-fall_to">-fall_to</a> to_list]
    [<a href="#opt-find_timing_paths-path_delay">-path_delay</a> min|min_rise|min_fall|max|max_rise|max_fall|min_max]
    [<a href="#opt-find_timing_paths-unconstrained">-unconstrained</a>]
    [<a href="#opt-find_timing_paths-scenes">-scenes</a> scenes]
    [<a href="#opt-find_timing_paths-group_path_count">-group_path_count</a> path_count]
    [<a href="#opt-find_timing_paths-endpoint_path_count">-endpoint_path_count</a> path_count]
    [<a href="#opt-find_timing_paths-unique_paths_to_endpoint">-unique_paths_to_endpoint</a>]
    [<a href="#opt-find_timing_paths-unique_edges_to_endpoint">-unique_edges_to_endpoint</a>]
    [<a href="#opt-find_timing_paths-slack_max">-slack_max</a> slack_max]
    [<a href="#opt-find_timing_paths-slack_min">-slack_min</a> slack_min]
    [<a href="#opt-find_timing_paths-sort_by_slack">-sort_by_slack</a>]
    [<a href="#opt-find_timing_paths-path_group">-path_group</a> group_name]</code></pre>

The `find_timing_paths` command returns a list of path objects for scripting. Use the `get_property` function to access properties of the paths.

### Options

`-from` {: #opt-find_timing_paths-from }
: Return paths from a list of clocks, instances, ports, register clock pins, or latch data pins.

`-rise_from` {: #opt-find_timing_paths-rise_from }
: Return paths from the rising edge of clocks, instances, ports, register clock pins, or latch data pins.

`-fall_from` {: #opt-find_timing_paths-fall_from }
: Return paths from the falling edge of clocks, instances, ports, register clock pins, or latch data pins.

`-through` {: #opt-find_timing_paths-through }
: Return paths through a list of instances, pins or nets.

`-rise_through` {: #opt-find_timing_paths-rise_through }
: Return rising paths through a list of instances, pins or nets.

`-fall_through` {: #opt-find_timing_paths-fall_through }
: Return falling paths through a list of instances, pins or nets.

`-to` {: #opt-find_timing_paths-to }
: Return paths to a list of clocks, instances, ports or pins.

`-rise_to` {: #opt-find_timing_paths-rise_to }
: Return rising paths to a list of clocks, instances, ports or pins.

`-fall_to` {: #opt-find_timing_paths-fall_to }
: Return falling paths to a list of clocks, instances, ports or pins.

`-path_delay` {: #opt-find_timing_paths-path_delay }
: - `min`: Return min path (hold) checks.
  - `min_rise`: Return min path (hold) checks for rising endpoints.
  - `min_fall`: Return min path (hold) checks for falling endpoints.
  - `max`: Return max path (setup) checks.
  - `max_rise`: Return max path (setup) checks for rising endpoints.
  - `max_fall`: Return max path (setup) checks for falling endpoints.
  - `min_max`: Return min and max path (setup and hold) checks.

`-unconstrained` {: #opt-find_timing_paths-unconstrained }
: Report unconstrained paths also.

`-scenes` {: #opt-find_timing_paths-scenes }
: `scenes`: Return paths for these scenes. The default is all scenes.

`-group_path_count` {: #opt-find_timing_paths-group_path_count }
: `path_count`: The number of paths to return in each path group.

`-endpoint_path_count` {: #opt-find_timing_paths-endpoint_path_count }
: `endpoint_path_count`: The number of paths to return for each endpoint.

`-unique_paths_to_endpoint` {: #opt-find_timing_paths-unique_paths_to_endpoint }
: Return multiple paths to an endpoint that traverse different pins without showing multiple paths with different rise/fall transitions.

`-unique_edges_to_endpoint` {: #opt-find_timing_paths-unique_edges_to_endpoint }
: When multiple paths to an endpoint are requested, only the worst path through the same pins and rise/fall edges is returned.

`-slack_max` {: #opt-find_timing_paths-slack_max }
: `max_slack`: Return paths with slack less than max_slack.

`-slack_min` {: #opt-find_timing_paths-slack_min }
: `min_slack`: Return paths with slack greater than min_slack.

`-sort_by_slack` {: #opt-find_timing_paths-sort_by_slack }
: Sort paths by slack rather than slack within path groups.

`-path_group` {: #opt-find_timing_paths-path_group }
: `groups`: Return paths in path groups. Paths in all groups are returned if this option is not specified.

## get_cells

<pre><code>get_cells
    [<a href="#opt-get_cells-hierarchical">-hierarchical</a>]
    [<a href="#opt-get_cells-hsc">-hsc</a> separator]
    [<a href="#opt-get_cells-filter">-filter</a> expr]
    [<a href="#opt-get_cells-regexp">-regexp</a>]
    [<a href="#opt-get_cells-nocase">-nocase</a>]
    [<a href="#opt-get_cells-quiet">-quiet</a>]
    [<a href="#opt-get_cells-of_objects">-of_objects</a> objects]
    [patterns]</code></pre>

The `get_cells` command returns a list of all cell instances that match patterns.

### Options

`-hierarchical` {: #opt-get_cells-hierarchical }
: Searches hierarchy levels below the current instance for matches.

`-hsc` {: #opt-get_cells-hsc }
: `separator`: Character to use to separate hierarchical instance names in patterns.

`-filter` {: #opt-get_cells-filter }
: A filter expression of the form
    "property==value"
  where property is a property supported by the `get_property` command.  See the section "Filter Expressions" for additional forms.

`-regexp` {: #opt-get_cells-regexp }
: Match patterns as regular expressions.

`-nocase` {: #opt-get_cells-nocase }
: Case-insensitive matching. Only valid with `-regexp`.

`-quiet` {: #opt-get_cells-quiet }
: Do not report an error if no objects match.

`-of_objects` {: #opt-get_cells-of_objects }
: The name of a pin or net, a list of pins returned by `get_pins`, or a list of nets returned by `get_nets`. The `-hierarchical` option cannot be used with `-of_objects`.

## get_clocks

<pre><code>get_clocks
    [<a href="#opt-get_clocks-regexp">-regexp</a>]
    [<a href="#opt-get_clocks-nocase">-nocase</a>]
    [<a href="#opt-get_clocks-quiet">-quiet</a>]
    [<a href="#opt-get_clocks-filter">-filter</a> expr]
    [patterns]</code></pre>

The `get_clocks` command returns a list of all clocks that have been defined.

### Options

`-regexp` {: #opt-get_clocks-regexp }
: Match patterns as regular expressions.

`-nocase` {: #opt-get_clocks-nocase }
: Case-insensitive matching. Only valid with `-regexp`.

`-quiet` {: #opt-get_clocks-quiet }
: Do not report an error if no objects match.

`-filter` {: #opt-get_clocks-filter }
: A filter expression of the form
    "property==value"
  where property is a property supported by the `get_property` command.  See the section "Filter Expressions" for additional forms.

## get_fanin

<pre><code>get_fanin
    <a href="#opt-get_fanin-to">-to</a>
    sink_list
    [<a href="#opt-get_fanin-flat">-flat</a>]
    [<a href="#opt-get_fanin-only_cells">-only_cells</a>]
    [<a href="#opt-get_fanin-startpoints_only">-startpoints_only</a>]
    [<a href="#opt-get_fanin-levels">-levels</a> level_count]
    [<a href="#opt-get_fanin-pin_levels">-pin_levels</a> pin_count]
    [<a href="#opt-get_fanin-trace_arcs">-trace_arcs</a> timing|enabled|all]</code></pre>

The `get_fanin`  command returns traverses the design from sink_list pins, ports or nets backwards and return the fanin pins or instances.

### Options

`-to` {: #opt-get_fanin-to }
: `sink_list`: List of pins, ports, or nets to find the fanin of. For nets, the fanin of driver pins on the nets are returned.

`-flat` {: #opt-get_fanin-flat }
: With `-flat` pins in the fanin at any hierarchy level are returned. Without `-flat` only pins at the same hierarchy level as the sinks are returned.

`-only_cells` {: #opt-get_fanin-only_cells }
: Return the instances connected to the pins in the fanin.

`-startpoints_only` {: #opt-get_fanin-startpoints_only }
: Only return pins that are startpoints.

`-levels` {: #opt-get_fanin-levels }
: `level_count`: Only return pins within level_count instance traversals.

`-pin_levels` {: #opt-get_fanin-pin_levels }
: `pin_count`: Only return pins within pin_count pin traversals.

`-trace_arcs` {: #opt-get_fanin-trace_arcs }
: - `timing`: Only trace through timing arcs that are not disabled.
  - `enabled`: Only trace through timing arcs that are not disabled.
  - `all`: Trace through all arcs, including disabled ones.

## get_fanout

<pre><code>get_fanout
    <a href="#opt-get_fanout-from">-from</a>
    source_list
    [<a href="#opt-get_fanout-flat">-flat</a>]
    [<a href="#opt-get_fanout-only_cells">-only_cells</a>]
    [<a href="#opt-get_fanout-endpoints_only">-endpoints_only</a>]
    [<a href="#opt-get_fanout-levels">-levels</a> level_count]
    [<a href="#opt-get_fanout-pin_levels">-pin_levels</a> pin_count]
    [<a href="#opt-get_fanout-trace_arcs">-trace_arcs</a> timing|enabled|all]</code></pre>

The `get_fanout`  command returns traverses the design from source_list pins, ports or nets backwards and return the fanout pins or instances.

### Options

`-from` {: #opt-get_fanout-from }
: `source_list`: List of pins, ports, or nets to find the fanout of. For nets, the fanout of load pins on the nets are returned.

`-flat` {: #opt-get_fanout-flat }
: With `-flat` pins in the fanin at any hierarchy level are returned. Without `-flat` only pins at the same hierarchy level as the sinks are returned.

`-only_cells` {: #opt-get_fanout-only_cells }
: Return the instances connected to the pins in the fanout.

`-endpoints_only` {: #opt-get_fanout-endpoints_only }
: Only return pins that are endpoints.

`-levels` {: #opt-get_fanout-levels }
: `level_count`: Only return pins within level_count instance traversals.

`-pin_levels` {: #opt-get_fanout-pin_levels }
: `pin_count`: Only return pins within pin_count pin traversals.

`-trace_arcs` {: #opt-get_fanout-trace_arcs }
: - `timing`: Only trace through timing arcs that are not disabled.
  - `enabled`: Only trace through timing arcs that are not disabled.
  - `all`: Trace through all arcs, including disabled ones.

## get_full_name

<pre><code>get_full_name
    object</code></pre>

Return the name of object. Equivalent to [`get_property` object full_name].

## get_lib_cells

<pre><code>get_lib_cells
    [<a href="#opt-get_lib_cells-hsc">-hsc</a> separator]
    [<a href="#opt-get_lib_cells-regexp">-regexp</a>]
    [<a href="#opt-get_lib_cells-nocase">-nocase</a>]
    [<a href="#opt-get_lib_cells-quiet">-quiet</a>]
    [<a href="#opt-get_lib_cells-filter">-filter</a> expr]
    [<a href="#opt-get_lib_cells-of_objects">-of_objects</a> objects]
    [patterns]</code></pre>

The `get_lib_cells` command returns a list of library cells that match pattern. The library name can be prepended to the cell name pattern with the separator character, which defaults to `hierarchy_separator`.

### Options

`-hsc` {: #opt-get_lib_cells-hsc }
: `separator`: Character that separates the library name and cell name in patterns. Defaults to '/'.

`-regexp` {: #opt-get_lib_cells-regexp }
: Match patterns as regular expressions.

`-nocase` {: #opt-get_lib_cells-nocase }
: Case-insensitive matching. Only valid with `-regexp`.

`-quiet` {: #opt-get_lib_cells-quiet }
: Do not report an error if no objects match.

`-filter` {: #opt-get_lib_cells-filter }
: A filter expression of the form
    "property==value"
  where property is a property supported by the `get_property` command.  See the section "Filter Expressions" for additional forms.

`-of_objects` {: #opt-get_lib_cells-of_objects }
: A list of instance objects.

## get_lib_pins

<pre><code>get_lib_pins
    [<a href="#opt-get_lib_pins-hsc">-hsc</a> separator]
    [<a href="#opt-get_lib_pins-regexp">-regexp</a>]
    [<a href="#opt-get_lib_pins-nocase">-nocase</a>]
    [<a href="#opt-get_lib_pins-quiet">-quiet</a>]
    [<a href="#opt-get_lib_pins-filter">-filter</a> expr]
    [<a href="#opt-get_lib_pins-of_objects">-of_objects</a> objects]
    [patterns]</code></pre>

The `get_lib_pins` command returns a list of library ports that match pattern.     Use separator to separate the library and cell name patterns from the port name in pattern.

### Options

`-hsc` {: #opt-get_lib_pins-hsc }
: `separator`: Character that separates the library name, cell name and port name in pattern. Defaults to '/'.

`-regexp` {: #opt-get_lib_pins-regexp }
: Match patterns as regular expressions.

`-nocase` {: #opt-get_lib_pins-nocase }
: Case-insensitive matching. Only valid with `-regexp`.

`-quiet` {: #opt-get_lib_pins-quiet }
: Do not report an error if no objects match.

`-filter` {: #opt-get_lib_pins-filter }
: A filter expression of the form
    "property==value"
  where property is a property supported by the `get_property` command.  See the section "Filter Expressions" for additional forms.

`-of_objects` {: #opt-get_lib_pins-of_objects }
: A list of library cell objects.

## get_libs

<pre><code>get_libs
    [<a href="#opt-get_libs-regexp">-regexp</a>]
    [<a href="#opt-get_libs-nocase">-nocase</a>]
    [<a href="#opt-get_libs-quiet">-quiet</a>]
    [<a href="#opt-get_libs-filter">-filter</a> expr]
    [patterns]</code></pre>

The `get_libs` command returns a list of clocks that match patterns.

### Options

`-regexp` {: #opt-get_libs-regexp }
: Match patterns as regular expressions.

`-nocase` {: #opt-get_libs-nocase }
: Case-insensitive matching. Only valid with `-regexp`.

`-quiet` {: #opt-get_libs-quiet }
: Do not report an error if no objects match.

`-filter` {: #opt-get_libs-filter }
: A filter expression of the form
    "property==value"
  where property is a property supported by the `get_property` command.  See the section "Filter Expressions" for additional forms.

## get_modes

<pre><code>get_modes
    [<a href="#opt-get_modes-filter">-filter</a> expr]
    [mode_name]</code></pre>

The `get_modes` command finds SDC modes matching a pattern.

### Options

`-filter` {: #opt-get_modes-filter }
: A filter expression. See the section "Filter Expressions".

## get_name

<pre><code>get_name
    object</code></pre>

Return the name of object. Equivalent to [`get_property` object name].

## get_nets

<pre><code>get_nets
    [<a href="#opt-get_nets-hierarchical">-hierarchical</a>]
    [<a href="#opt-get_nets-hsc">-hsc</a> separator]
    [<a href="#opt-get_nets-regexp">-regexp</a>]
    [<a href="#opt-get_nets-nocase">-nocase</a>]
    [<a href="#opt-get_nets-quiet">-quiet</a>]
    [<a href="#opt-get_nets-filter">-filter</a> expr]
    [<a href="#opt-get_nets-of_objects">-of_objects</a> objects]
    [patterns]</code></pre>

The `get_nets` command returns a list of all nets that match patterns.

### Options

`-hierarchical` {: #opt-get_nets-hierarchical }
: Searches hierarchy levels below the current instance for matches.

`-hsc` {: #opt-get_nets-hsc }
: `separator`: Character that separates the library name, cell name and port name in pattern. Defaults to '/'.

`-regexp` {: #opt-get_nets-regexp }
: Match patterns as regular expressions.

`-nocase` {: #opt-get_nets-nocase }
: Case-insensitive matching. Only valid with `-regexp`.

`-quiet` {: #opt-get_nets-quiet }
: Do not report an error if no objects match.

`-filter` {: #opt-get_nets-filter }
: A filter expression of the form
    "property==value"
  where property is a property supported by the `get_property` command.  See the section "Filter Expressions" for additional forms.

`-of_objects` {: #opt-get_nets-of_objects }
: The name of a pin or instance, a list of pins returned by `get_pins`, or a list of instances returned by `get_cells`. The `-hierarchical` option cannot be used with `-of_objects`.

## get_pins

<pre><code>get_pins
    [<a href="#opt-get_pins-hierarchical">-hierarchical</a>]
    [<a href="#opt-get_pins-hsc">-hsc</a> separator]
    [<a href="#opt-get_pins-quiet">-quiet</a>]
    [<a href="#opt-get_pins-filter">-filter</a> expr]
    [<a href="#opt-get_pins-regexp">-regexp</a>]
    [<a href="#opt-get_pins-nocase">-nocase</a>]
    [<a href="#opt-get_pins-of_objects">-of_objects</a> objects]
    [patterns]</code></pre>

The `get_pins` command returns a list of all instance pins that match patterns.

A useful idiom to find the driver pin for a net is the following.

```
get_pins -of_objects [get_net net_name] -filter "direction==output"
```

### Options

`-hierarchical` {: #opt-get_pins-hierarchical }
: Searches hierarchy levels below the current instance for matches.

`-hsc` {: #opt-get_pins-hsc }
: `separator`: Character that separates the library name, cell name and port name in pattern. Defaults to '/'.

`-quiet` {: #opt-get_pins-quiet }
: Do not report an error if no objects match.

`-filter` {: #opt-get_pins-filter }
: A filter expression of the form
    "property==value"
  where property is a property supported by the `get_property` command.  See the section "Filter Expressions" for additional forms.

`-regexp` {: #opt-get_pins-regexp }
: Match patterns as regular expressions.

`-nocase` {: #opt-get_pins-nocase }
: Case-insensitive matching. Only valid with `-regexp`.

`-of_objects` {: #opt-get_pins-of_objects }
: The name of a net or instance, a list of nets returned by `get_nets`, or a list of instances returned by `get_cells`. The `-hierarchical` option cannot be used with `-of_objects`.

## get_ports

<pre><code>get_ports
    [<a href="#opt-get_ports-quiet">-quiet</a>]
    [<a href="#opt-get_ports-filter">-filter</a> expr]
    [<a href="#opt-get_ports-regexp">-regexp</a>]
    [<a href="#opt-get_ports-nocase">-nocase</a>]
    [<a href="#opt-get_ports-of_objects">-of_objects</a> objects]
    [patterns]</code></pre>

The `get_ports` command returns a list of all top level ports that match patterns.

### Options

`-quiet` {: #opt-get_ports-quiet }
: Do not report an error if no objects match.

`-filter` {: #opt-get_ports-filter }
: A filter expression of the form
    "property==value"
  where property is a property supported by the `get_property` command.  See the section "Filter Expressions" for additional forms.

`-regexp` {: #opt-get_ports-regexp }
: Match patterns as regular expressions.

`-nocase` {: #opt-get_ports-nocase }
: Case-insensitive matching. Only valid with `-regexp`.

`-of_objects` {: #opt-get_ports-of_objects }
: The name of  net or a list of nets returned by `get_nets`.

## get_property

<pre><code>get_property
    [<a href="#opt-get_property-object_type">-object_type</a> library|liberty_library|cell|liberty_cell|instance|pin|net|port|clock|timing_arc]
    object
    property</code></pre>

The `get_property` command returns a property of an object. Properties for each object type are shown below.

| Object type | Properties |
| --- | --- |
| cell (SDC lib_cell) | `base_name`, `filename`, `full_name`, `library`, `name` |
| clock | `full_name`, `is_generated`, `is_propagated`, `is_virtual`, `name`, `period`, `sources` |
| edge | `delay_max_fall`, `delay_min_fall`, `delay_max_rise`, `delay_min_rise`, `full_name`, `from_pin`, `sense`, `to_pin` |
| instance (SDC cell) | `cell`, `full_name`, `is_buffer`, `is_clock_gate`, `is_hierarchical`, `is_inverter`, `is_macro`, `is_memory`, `liberty_cell`, `name`, `ref_name` |
| liberty_cell (SDC lib_cell) | `area`, `base_name`, `dont_use`, `filename`, `full_name`, `is_buffer`, `is_inverter`, `is_memory`, `library`, `name` |
| liberty_port (SDC lib_pin) | `capacitance`, `direction`, `drive_resistance`, `drive_resistance_max_fall`, `drive_resistance_max_rise`, `drive_resistance_min_fall`, `drive_resistance_min_rise`, `full_name`, `intrinsic_delay`, `intrinsic_delay_max_fall`, `intrinsic_delay_max_rise`, `intrinsic_delay_min_fall`, `intrinsic_delay_min_rise`, `is_register_clock`, `lib_cell`, `name` |
| library | `filename` (Liberty library only), `name`, `full_name` |
| mode | `name`, `full_name` |
| net | `full_name`, `name` |
| path (PathEnd) | `endpoint`, `endpoint_clock`, `endpoint_clock_pin`, `slack`, `startpoint`, `startpoint_clock`, `points` |
| pin | `activity`, `slew_max_fall`, `slew_max_rise`, `slew_min_fall`, `slew_min_rise`, `clocks`, `clock_domains`, `direction`, `full_name`, `is_hierarchical`, `is_port`, `is_register_clock`, `lib_pin_name`, `name`, `slack_max`, `slack_max_fall`, `slack_max_rise`, `slack_min`, `slack_min_fall`, `slack_min_rise` |
| point (PathRef) | `arrival`, `pin`, `required`, `slack` |
| port | `activity`, `slew_max_fall`, `slew_max_rise`, `slew_min_fall`, `slew_min_rise`, `direction`, `full_name`, `liberty_port`, `name`, `slack_max`, `slack_max_fall`, `slack_max_rise`, `slack_min`, `slack_min_fall`, `slack_min_rise` |
| scene | `name`, `full_name` |

The pin `activity` property is a list of activity (transitions per second), duty cycle, and origin. Origin is one of `global` (`set_power_activity -global`), `input` (`set_power_activity -input`), `user` (`set_power_activity -input_ports`/`-pins`), `vcd` (`read_vcd`), `saif` (`read_saif`), `propagated`, `clock` (`create_clock`/`create_generated_clock`), or `constant` (Verilog tie high/low, `set_case_analysis`, `set_logic_one`/`zero`/`dc`).

### Options

`-object_type` {: #opt-get_property-object_type }
: `object_type`: The type of object when it is specified as a name.
  cell|pin|net|port|clock|library|library_cell|library_pin|timing_arc

## get_scenes

<pre><code>get_scenes
    [<a href="#opt-get_scenes-modes">-modes</a> mode_names]
    [<a href="#opt-get_scenes-filter">-filter</a> expr]
    scene_names</code></pre>

The `get_scenes` command is used to find the scenes matching a pattern or that use an SDC mode.

### Options

`-modes` {: #opt-get_scenes-modes }
: Return scenes that use these SDC modes.

`-filter` {: #opt-get_scenes-filter }
: A filter expression. See the section "Filter Expressions".

## get_timing_edges

<pre><code>get_timing_edges
    [<a href="#opt-get_timing_edges-from">-from</a> from_pin]
    [<a href="#opt-get_timing_edges-to">-to</a> to_pin]
    [<a href="#opt-get_timing_edges-of_objects">-of_objects</a> objects]
    [<a href="#opt-get_timing_edges-filter">-filter</a> expr]</code></pre>

The `get_timing_edges` command returns a list of timing edges (arcs) to, from or between pins. The result can be passed to `get_property` or `set_disable_timing`.

### Options

`-from` {: #opt-get_timing_edges-from }
: `from_pin`: A list of pins.

`-to` {: #opt-get_timing_edges-to }
: `to_pin`: A list of pins.

`-of_objects` {: #opt-get_timing_edges-of_objects }
: A list of instances or library cells. The `-from` and `-to` options cannot be used with `-of_objects`.

`-filter` {: #opt-get_timing_edges-filter }
: A filter expression of the form
    "property==value"
  where property is a property supported by the `get_property` command.  See the section "Filter Expressions" for additional forms.

## group_path

<pre><code>group_path
    <a href="#opt-group_path-name">-name</a>
    group_name
    [<a href="#opt-group_path-weight">-weight</a> weight]
    [<a href="#opt-group_path-critical_range">-critical_range</a> range]
    [<a href="#opt-group_path-default">-default</a>]
    [<a href="#opt-group_path-comment">-comment</a> comment]
    [<a href="#opt-group_path-from">-from</a> from_list]
    [<a href="#opt-group_path-rise_from">-rise_from</a> from_list]
    [<a href="#opt-group_path-fall_from">-fall_from</a> from_list]
    [<a href="#opt-group_path-through">-through</a> through_list]
    [<a href="#opt-group_path-rise_through">-rise_through</a> through_list]
    [<a href="#opt-group_path-fall_through">-fall_through</a> through_list]
    [<a href="#opt-group_path-to">-to</a> to_list]
    [<a href="#opt-group_path-rise_to">-rise_to</a> to_list]
    [<a href="#opt-group_path-fall_to">-fall_to</a> to_list]</code></pre>

The `group_path` command is used to group paths reported by the `report_checks` command. See `set_false_path` for a description of allowed from_list, through_list and to_list objects.

### Options

`-name` {: #opt-group_path-name }
: `group_name`: The name of the path group.

`-weight` {: #opt-group_path-weight }
: `weight`: Not supported.

`-critical_range` {: #opt-group_path-critical_range }
: `range`: Not supported.

`-default` {: #opt-group_path-default }
: Restore the paths in the path group `-from`/`-to`/`-through`/`-to` to their default path group.

`-comment` {: #opt-group_path-comment }
: Comment string saved with the constraint.

`-from` {: #opt-group_path-from }
: Group paths from a list of clocks, instances, ports, register clock pins, or latch data pins.

`-rise_from` {: #opt-group_path-rise_from }
: Group  paths from the rising edge of clocks, instances, ports, register clock pins, or latch data pins.

`-fall_from` {: #opt-group_path-fall_from }
: Group paths from the falling edge of clocks, instances, ports, register clock pins, or latch data pins.

`-through` {: #opt-group_path-through }
: Group paths through a list of instances, pins or nets.

`-rise_through` {: #opt-group_path-rise_through }
: Group rising paths through a list of instances, pins or nets.

`-fall_through` {: #opt-group_path-fall_through }
: Group falling paths through a list of instances, pins or nets.

`-to` {: #opt-group_path-to }
: Group paths to a list of clocks, instances, ports or pins.

`-rise_to` {: #opt-group_path-rise_to }
: Group rising paths to a list of clocks, instances, ports or pins.

`-fall_to` {: #opt-group_path-fall_to }
: Group falling paths to a list of clocks, instances, port-s or pins.

## help

<pre><code>help
    [<a href="#opt-help-verbose">-verbose</a>]
    [pattern]</code></pre>

Print command usage. With a single match, print the description and options. Use `-verbose` to print full `help` for every match.

### Options

`-verbose` {: #opt-help-verbose }
: Print full descriptions even when multiple commands match.

## include

<pre><code>include
    [<a href="#opt-include-echo">-e</a>|<a href="#opt-include-echo">-echo</a>]
    [<a href="#opt-include-verbose">-v</a>|<a href="#opt-include-verbose">-verbose</a>]
    filename
    [&gt; filename]
    [&gt;&gt; filename]</code></pre>

Read STA/SDC/Tcl commands from filename.

The `include` command stops and reports any errors encountered while reading a file unless `sta_continue_on_error` is 1.

### Options

`-echo` {: #opt-include-echo }
: Print each command before evaluating it.

`-verbose` {: #opt-include-verbose }
: Print each command before evaluating it as well as the result it returns.

## link_design

<pre><code>link_design
    [<a href="#opt-link_design-no_black_boxes">-no_black_boxes</a>]
    [top_cell_name]</code></pre>

Link (elaborate, flatten) the top-level cell `cell_name`. The design must be linked after reading netlist and library files. The default value of `cell_name` is the current design.

By default the linker creates empty black-box cells for instances that reference undefined cells. Use `-no_black_boxes` to report an error and fail the link instead.

The `link_design` command returns 1 if the link succeeds and 0 if it fails.

### Options

`-no_black_boxes` {: #opt-link_design-no_black_boxes }
: Do not make empty "black box" cells for instances that reference undefined cells.

## log_begin

<pre><code>log_begin
    filename</code></pre>

The `log_begin` command copies all subsequent command output to a file until `log_end` is called.

## log_end

<pre><code>log_end</code></pre>

The `log_end` command stops copying command output to a file started with `log_begin`.

## make_instance

<pre><code>make_instance
    inst_path
    lib_cell</code></pre>

The `make_instance` command makes an instance of library cell lib_cell.

## make_net

<pre><code>make_net
    net_path</code></pre>

Creates a net for each hierarchical net name.

## make_port

<pre><code>make_port
    port_name
    direction</code></pre>

The `make_port` command creates a port on the top-level cell. `direction` is `input`, `output`, `bidirect`, `tristate`, `internal`, `power`, or `ground`.

## read_liberty

<pre><code>read_liberty
    [<a href="#opt-read_liberty-corner">-corner</a> corner]
    [<a href="#opt-read_liberty-min">-min</a>]
    [<a href="#opt-read_liberty-max">-max</a>]
    [<a href="#opt-read_liberty-infer_latches">-infer_latches</a>]
    filename</code></pre>

The `read_liberty` command reads a Liberty format library file. The first library that is read sets the units used by SDC/Tcl commands and reporting. The include_file attribute is supported.

Some Liberty libraries do not include latch groups for cells that describe transparent latches. In that situation the `-infer_latches` command flag can be used to infer the latches. The timing arcs required for a latch to be inferred should look like the following:

```
cell (inferred_latch) {
  pin(D) {
    direction : input ;
    timing () {
      related_pin : "E" ;
      timing_type : setup_falling ;
    }
    timing () {
      related_pin : "E" ;
      timing_type : hold_falling ;
    }
  }
  pin(E) {
    direction : input;
  }
  pin(Q) {
    direction : output ;
    timing () {
      related_pin : "D" ;
    }
    timing () {
      related_pin : "E" ;
      timing_type : rising_edge ;
    }
  }
}
```

In this example a positive level-sensitive latch is inferred.

Files compressed with gzip are automatically uncompressed.

### Options

`-corner` {: #opt-read_liberty-corner }
: Deprecated. Use `define_scene` to assign Liberty libraries to a scene.

`-min` {: #opt-read_liberty-min }
: Use the library for min-delay (hold) analysis.

`-max` {: #opt-read_liberty-max }
: Use the library for max-delay (setup) analysis.

`-infer_latches` {: #opt-read_liberty-infer_latches }
: Infer latches from timing arcs when the Liberty file has no latch groups.

## read_power_activities

<pre><code>read_power_activities
    [<a href="#opt-read_power_activities-scope">-scope</a> scope]
    <a href="#opt-read_power_activities-vcd">-vcd</a>
    filename</code></pre>

The `read_power_activities` command is deprecated. Use `read_vcd` instead.

### Options

`-scope` {: #opt-read_power_activities-scope }
: The VCD scope of the current design. Typically the test bench name and design under test instance name. Scope levels are separated with '/'.

`-vcd` {: #opt-read_power_activities-vcd }
: VCD file to read. Use `read_vcd` instead.

## read_saif

<pre><code>read_saif
    [<a href="#opt-read_saif-scope">-scope</a> scope]
    filename</code></pre>

The `read_saif` command reads a SAIF (Switching Activity Interchange Format) file from a Verilog simulation and extracts pin activities and duty cycles for use in power estimation. Files compressed with gzip are supported. Annotated activities are propagated to the fanout of the annotated pins.

### Options

`-scope` {: #opt-read_saif-scope }
: The SAIF scope of the current design to extract simulation data. Typically the test bench name and design under test instance name. Scope levels are separated with '/'.

## read_sdc

<pre><code>read_sdc
    [<a href="#opt-read_sdc-echo">-echo</a>]
    [<a href="#opt-read_sdc-mode">-mode</a> mode_name]
    filename</code></pre>

Read SDC commands from filename.

If the mode does not exist it is created. Multiple SDC files can append commands to a mode by using the `-mode_name` argument for each one. If no `-mode` arguement is is used the commands are added to the current  mode.

The `read_sdc` command stops and reports any errors encountered while reading a file unless `sta_continue_on_error` is 1.

Files compressed with gzip are automatically uncompressed.

### Options

`-echo` {: #opt-read_sdc-echo }
: Print each command before evaluating it.

`-mode` {: #opt-read_sdc-mode }
: Mode for the SDC commands in the file.

## read_sdf

<pre><code>read_sdf
    [<a href="#opt-read_sdf-path">-path</a> path]
    [<a href="#opt-read_sdf-scene">-scene</a> scene]
    [<a href="#opt-read_sdf-cond_use">-cond_use</a> min|max|min_max]
    [<a href="#opt-read_sdf-unescaped_dividers">-unescaped_dividers</a>]
    filename</code></pre>

Read SDF delays from a file. The min and max values in the SDF tuples are used to annotate delays. Typical values in the SDF tuples are ignored. If multiple scenes are defined `-scene` must be specified. SDC annotation for MCMM analysis must follow the scene definitions.

Files compressed with gzip are automatically uncompressed.

INCREMENT is supported as an alias for INCREMENTAL.

The following SDF statements are not supported.

```
PORT
INSTANCE wildcards
```

### Options

`-path` {: #opt-read_sdf-path }
: Hierarchical instance path prefix for SDF annotation.

`-scene` {: #opt-read_sdf-scene }
: Scene delays to annotate.

`-cond_use` {: #opt-read_sdf-cond_use }
: - `min`: Use SDF COND delays for min analysis.
  - `max`: Use COND delays for max analysis.
  - `min_max`: Use COND delays for min and max analysis.

`-unescaped_dividers` {: #opt-read_sdf-unescaped_dividers }
: With this option path names in the SDF do not have to escape hierarchy dividers when the path name is escaped. For example, the escaped Verilog name "\inst1/inst2 " can be referenced as "inst1/inst2". The correct SDF name is "inst1\/inst2", since the divider does not represent a change in hierarchy in this case.

## read_spef

<pre><code>read_spef
    [<a href="#opt-read_spef-name">-name</a> spef_name]
    [<a href="#opt-read_spef-corner">-corner</a> corner]
    [<a href="#opt-read_spef-min">-min</a>]
    [<a href="#opt-read_spef-max">-max</a>]
    [<a href="#opt-read_spef-path">-path</a> path]
    [<a href="#opt-read_spef-pin_cap_included">-pin_cap_included</a>]
    [<a href="#opt-read_spef-keep_capacitive_coupling">-keep_capacitive_coupling</a>]
    [<a href="#opt-read_spef-coupling_reduction_factor">-coupling_reduction_factor</a> factor]
    [<a href="#opt-read_spef-reduce">-reduce</a>]
    filename</code></pre>

The `read_spef` command reads a file of net parasitics in SPEF format. Use the `-report_parasitic_annotation` command to check for nets that are not annotated.

Files compressed with gzip are automatically uncompressed.

Separate min/max parasitics can be annotated for each scene.

```
read_spef -name min spef1
read_spef -name max spef2
define_scene scene1 -mode mode1 -spef_min min -spef_max max
```

Coupling capacitors are multiplied by the `-coupling_reduction_factor` when a parasitic network is reduced.

The following SPEF constructs are ignored.

```
*DESIGN_FLOW (all values are ignored)
*S slews
*D driving cell
*I pin capacitances (library cell capacitances are used instead)
*Q r_net load poles
*K r_net load residues
```

If the SPEF file contains triplet values the first value is used.

Parasitic networks (DSPEF) can be annotated on hierarchical blocks using the `-path` argument to specify the instance path to the block. Parasitic networks in the higher level netlist are stitched together at the hierarchical pins of the blocks.

### Options

`-name` {: #opt-read_spef-name }
: The name of the SPEF parasitics to use for defining scenes. The default is the base name of filename.

`-corner` {: #opt-read_spef-corner }
: Process corner to annotate. Deprecated; use `-name` and `define_scene`.

`-min` {: #opt-read_spef-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-read_spef-max }
: Apply to maximum (setup) analysis.

`-path` {: #opt-read_spef-path }
: Hierarchical block instance path to annotate with  parasitics.

`-pin_cap_included` {: #opt-read_spef-pin_cap_included }
: SPEF pin capacitances are included (library pin capacitances are not added).

`-keep_capacitive_coupling` {: #opt-read_spef-keep_capacitive_coupling }
: Keep coupling capacitors in parasitic networks rather than converting them to grounded capacitors.

`-coupling_reduction_factor` {: #opt-read_spef-coupling_reduction_factor }
: `factor`: Factor to multiply coupling capacitance by when reducing parasitic networks. The default value is 1.0.

`-reduce` {: #opt-read_spef-reduce }
: Reduce parasitic networks to the form used by the current delay calculator.

## read_vcd

<pre><code>read_vcd
    [<a href="#opt-read_vcd-scope">-scope</a> scope]
    [<a href="#opt-read_vcd-mode">-mode</a> mode_name]
    [<a href="#opt-read_vcd-begin_time">-begin_time</a> begin_time]
    [<a href="#opt-read_vcd-end_time">-end_time</a> end_time]
    filename</code></pre>

The `read_vcd` command reads a VCD (Value Change Dump) file from a Verilog simulation and extracts pin activities and duty cycles for use in power estimation. Files compressed with gzip are supported. Annotated activities are propagated to the fanout of the annotated pins.

### Options

`-scope` {: #opt-read_vcd-scope }
: The VCD scope of the current design to extract simulation data. Typically the test bench name and design under test instance name. Scope levels are separated with '/'.

`-mode` {: #opt-read_vcd-mode }
: Mode to annotate activities.

`-begin_time` {: #opt-read_vcd-begin_time }
: Ignore VCD activity before this time.

`-end_time` {: #opt-read_vcd-end_time }
: Ignore VCD activity after this time.

## read_verilog

<pre><code>read_verilog
    filename</code></pre>

The `read_verilog` command reads a gate level verilog netlist. After all verilog netlist and Liberty libraries are read the design must be linked with the `link_design` command.

Verilog 2001 module port declaratations are supported. An example is shown below.

```
module top (input in1, in2, clk1, clk2, clk3,
            output out);
```

Files compressed with gzip are automatically uncompressed.

## replace_cell

<pre><code>replace_cell
    instance
    lib_cell</code></pre>

The `replace_cell` command changes the cell of an instance. The replacement cell must have the same port list (number, name, and order) as the instance's existing cell for the replacement to be successful.

## report_activity_annotation

<pre><code>report_activity_annotation
    [<a href="#opt-report_activity_annotation-report_unannotated">-report_unannotated</a>]
    [<a href="#opt-report_activity_annotation-report_annotated">-report_annotated</a>]</code></pre>

Report a summary of pins that are annotated by `read_vcd`, `read_saif` or `set_power_activity`. Sequential internal pins and hierarchical pins are ignored.

### Options

`-report_unannotated` {: #opt-report_activity_annotation-report_unannotated }
: Report unannotated pins.

`-report_annotated` {: #opt-report_activity_annotation-report_annotated }
: Report annotated pins.

## report_annotated_check

<pre><code>report_annotated_check
    [<a href="#opt-report_annotated_check-setup">-setup</a>]
    [<a href="#opt-report_annotated_check-hold">-hold</a>]
    [<a href="#opt-report_annotated_check-recovery">-recovery</a>]
    [<a href="#opt-report_annotated_check-removal">-removal</a>]
    [<a href="#opt-report_annotated_check-nochange">-nochange</a>]
    [<a href="#opt-report_annotated_check-width">-width</a>]
    [<a href="#opt-report_annotated_check-period">-period</a>]
    [<a href="#opt-report_annotated_check-max_skew">-max_skew</a>]
    [<a href="#opt-report_annotated_check-scene">-scene</a> scene]
    [<a href="#opt-report_annotated_check-max_lines">-max_lines</a> lines]
    [<a href="#opt-report_annotated_check-report_annotated">-report_annotated</a>]
    [<a href="#opt-report_annotated_check-report_unannotated">-report_unannotated</a>]
    [<a href="#opt-report_annotated_check-constant_arcs">-constant_arcs</a>]</code></pre>

The `report_annotated_check` command reports a summary of SDF timing check annotation. The `-report_annotated` and `-report_annotated` options can be used to list arcs that are annotated or not annotated.

### Options

`-setup` {: #opt-report_annotated_check-setup }
: Apply to setup checks.

`-hold` {: #opt-report_annotated_check-hold }
: Apply to hold checks.

`-recovery` {: #opt-report_annotated_check-recovery }
: Report annotated recovery checks.

`-removal` {: #opt-report_annotated_check-removal }
: Report annotated removal checks.

`-nochange` {: #opt-report_annotated_check-nochange }
: Report annotated nochange checks.

`-width` {: #opt-report_annotated_check-width }
: Report annotated width checks.

`-period` {: #opt-report_annotated_check-period }
: Report annotated period checks.

`-max_skew` {: #opt-report_annotated_check-max_skew }
: Report annotated max skew checks.

`-scene` {: #opt-report_annotated_check-scene }
: Restrict the command to one scene.

`-max_lines` {: #opt-report_annotated_check-max_lines }
: `lines`: Maximum number of lines listed by the `-report_annotated` and `-report_unannotated` options.

`-report_annotated` {: #opt-report_annotated_check-report_annotated }
: Report annotated timing arcs.

`-report_unannotated` {: #opt-report_annotated_check-report_unannotated }
: Report unannotated timing arcs.

`-constant_arcs` {: #opt-report_annotated_check-constant_arcs }
: Report separate annotation counts for arcs disabled by logic constants (`set_logic_one`, `set_logic_zero`).

## report_annotated_delay

<pre><code>report_annotated_delay
    [<a href="#opt-report_annotated_delay-cell">-cell</a>]
    [<a href="#opt-report_annotated_delay-net">-net</a>]
    [<a href="#opt-report_annotated_delay-from_in_ports">-from_in_ports</a>]
    [<a href="#opt-report_annotated_delay-to_out_ports">-to_out_ports</a>]
    [<a href="#opt-report_annotated_delay-scene">-scene</a> scene]
    [<a href="#opt-report_annotated_delay-max_lines">-max_lines</a> lines]
    [<a href="#opt-report_annotated_delay-report_annotated">-report_annotated</a>]
    [<a href="#opt-report_annotated_delay-report_unannotated">-report_unannotated</a>]
    [<a href="#opt-report_annotated_delay-constant_arcs">-constant_arcs</a>]</code></pre>

The `report_annotated_delay` command reports a summary of SDF delay annotation. Without the `-from_in_ports` and `-to_out_ports` options arcs to and from top level ports are not reported. The `-report_annotated` and `-report_unannotated` options can be used to list arcs that are annotated or not annotated.

### Options

`-cell` {: #opt-report_annotated_delay-cell }
: Report annotated cell delays.

`-net` {: #opt-report_annotated_delay-net }
: Report annotated internal net delays.

`-from_in_ports` {: #opt-report_annotated_delay-from_in_ports }
: Report annotated delays from input ports.

`-to_out_ports` {: #opt-report_annotated_delay-to_out_ports }
: Report annotated delays to output ports.

`-scene` {: #opt-report_annotated_delay-scene }
: Restrict the command to one scene.

`-max_lines` {: #opt-report_annotated_delay-max_lines }
: `lines`: Maximum number of lines listed by the `-report_annotated` and `-report_unannotated` options.

`-report_annotated` {: #opt-report_annotated_delay-report_annotated }
: Report annotated timing arcs.

`-report_unannotated` {: #opt-report_annotated_delay-report_unannotated }
: Report unannotated timing arcs.

`-constant_arcs` {: #opt-report_annotated_delay-constant_arcs }
: Report separate annotation counts for arcs disabled by logic constants (`set_logic_one`, `set_logic_zero`).

## report_arrival

<pre><code>report_arrival
    [<a href="#opt-report_arrival-scene">-scene</a> scene]
    [<a href="#opt-report_arrival-report_variance">-report_variance</a>]
    [<a href="#opt-report_arrival-digits">-digits</a> digits]
    pin</code></pre>

The `report_arrival` command reports min/max rise/fall arrival times at a pin with respect to each clock that has a path to the pin.

### Options

`-scene` {: #opt-report_arrival-scene }
: Restrict the command to one scene.

`-report_variance` {: #opt-report_arrival-report_variance }
: Include delay distribution variance in the report.

`-digits` {: #opt-report_arrival-digits }
: Number of digits to print after the decimal point.

## report_check_types

<pre><code>report_check_types
    [<a href="#opt-report_check_types-scenes">-scenes</a> scenes]
    [<a href="#opt-report_check_types-violators">-violators</a>]
    [<a href="#opt-report_check_types-verbose">-verbose</a>]
    [<a href="#opt-report_check_types-format">-format</a> slack_only|end]
    [<a href="#opt-report_check_types-max_delay">-max_delay</a>]
    [<a href="#opt-report_check_types-min_delay">-min_delay</a>]
    [<a href="#opt-report_check_types-recovery">-recovery</a>]
    [<a href="#opt-report_check_types-removal">-removal</a>]
    [<a href="#opt-report_check_types-clock_gating_setup">-clock_gating_setup</a>]
    [<a href="#opt-report_check_types-clock_gating_hold">-clock_gating_hold</a>]
    [<a href="#opt-report_check_types-max_slew">-max_slew</a>]
    [<a href="#opt-report_check_types-min_slew">-min_slew</a>]
    [<a href="#opt-report_check_types-max_fanout">-max_fanout</a>]
    [<a href="#opt-report_check_types-min_fanout">-min_fanout</a>]
    [<a href="#opt-report_check_types-max_capacitance">-max_capacitance</a>]
    [<a href="#opt-report_check_types-min_capacitance">-min_capacitance</a>]
    [<a href="#opt-report_check_types-min_pulse_width">-min_pulse_width</a>]
    [<a href="#opt-report_check_types-min_period">-min_period</a>]
    [<a href="#opt-report_check_types-max_skew">-max_skew</a>]
    [<a href="#opt-report_check_types-net">-net</a> net]
    [<a href="#opt-report_check_types-max_count">-max_count</a> max_count]
    [<a href="#opt-report_check_types-digits">-digits</a> digits]
    [<a href="#opt-report_check_types-no_line_splits">-no_line_splits</a>]
    [&gt; filename]
    [&gt;&gt; filename]</code></pre>

The `report_check_types` command reports the slack for each type of timing and design rule constraint. The keyword options allow a subset of the constraint types to be reported.

### Options

`-scenes` {: #opt-report_check_types-scenes }
: Report checks for some scenes. The default value is all scenes.

`-violators` {: #opt-report_check_types-violators }
: Report all violated timing and design rule constraints.

`-verbose` {: #opt-report_check_types-verbose }
: Use a verbose output format.

`-format` {: #opt-report_check_types-format }
: - `slack_only`: Report the minimum slack for each timing check.
  - `end`: Report the endpoint for each check.

`-max_delay` {: #opt-report_check_types-max_delay }
: Report setup and max delay path delay constraints.

`-min_delay` {: #opt-report_check_types-min_delay }
: Report hold and min delay path delay constraints.

`-recovery` {: #opt-report_check_types-recovery }
: Report asynchronous recovery checks.

`-removal` {: #opt-report_check_types-removal }
: Report asynchronous removal checks.

`-clock_gating_setup` {: #opt-report_check_types-clock_gating_setup }
: Report gated clock enable setup checks.

`-clock_gating_hold` {: #opt-report_check_types-clock_gating_hold }
: Report gated clock hold setup checks.

`-max_slew` {: #opt-report_check_types-max_slew }
: Report max transition design rule checks.

`-min_slew` {: #opt-report_check_types-min_slew }
: Report min slew design rule checks.

`-max_fanout` {: #opt-report_check_types-max_fanout }
: Report max fanout design rule checks.

`-min_fanout` {: #opt-report_check_types-min_fanout }
: Report min fanout design rule checks.

`-max_capacitance` {: #opt-report_check_types-max_capacitance }
: Report max capacitance design rule checks.

`-min_capacitance` {: #opt-report_check_types-min_capacitance }
: Report min capacitance design rule checks.

`-min_pulse_width` {: #opt-report_check_types-min_pulse_width }
: Report min pulse width design rule checks.

`-min_period` {: #opt-report_check_types-min_period }
: Report min period design rule checks.

`-max_skew` {: #opt-report_check_types-max_skew }
: Report max skew design rule checks.

`-net` {: #opt-report_check_types-net }
: Report checks on this net.

`-max_count` {: #opt-report_check_types-max_count }
: Maximum number of checks to report.

`-digits` {: #opt-report_check_types-digits }
: Number of digits to print after the decimal point.

`-no_line_splits` {: #opt-report_check_types-no_line_splits }
: Do not split long lines into multiple lines.

## report_checks

<pre><code>report_checks
    [<a href="#opt-report_checks-from">-from</a> from_list|<a href="#opt-report_checks-rise_from">-rise_from</a> from_list|<a href="#opt-report_checks-fall_from">-fall_from</a> from_list]
    [<a href="#opt-report_checks-through">-through</a> through_list|<a href="#opt-report_checks-rise_through">-rise_through</a> through_list|<a href="#opt-report_checks-fall_through">-fall_through</a> through_list]
    [<a href="#opt-report_checks-to">-to</a> to_list|<a href="#opt-report_checks-rise_to">-rise_to</a> to_list|<a href="#opt-report_checks-fall_to">-fall_to</a> to_list]
    [<a href="#opt-report_checks-unconstrained">-unconstrained</a>]
    [<a href="#opt-report_checks-path_delay">-path_delay</a> min|min_rise|min_fall|max|max_rise|max_fall|min_max]
    [<a href="#opt-report_checks-scenes">-scenes</a> scenes]
    [<a href="#opt-report_checks-group_path_count">-group_path_count</a> path_count]
    [<a href="#opt-report_checks-endpoint_path_count">-endpoint_path_count</a> path_count]
    [<a href="#opt-report_checks-unique_paths_to_endpoint">-unique_paths_to_endpoint</a>]
    [<a href="#opt-report_checks-unique_edges_to_endpoint">-unique_edges_to_endpoint</a>]
    [<a href="#opt-report_checks-slack_max">-slack_max</a> slack_max]
    [<a href="#opt-report_checks-slack_min">-slack_min</a> slack_min]
    [<a href="#opt-report_checks-sort_by_slack">-sort_by_slack</a>]
    [<a href="#opt-report_checks-path_group">-path_group</a> group_name]
    [<a href="#opt-report_checks-format">-format</a> full|full_clock|full_clock_expanded|short|end|slack_only|summary|json]
    [<a href="#opt-report_checks-fields">-fields</a> capacitance|slew|fanout|input_pin|net|src_attr|variation]
    [<a href="#opt-report_checks-digits">-digits</a> digits]
    [<a href="#opt-report_checks-no_line_splits">-no_line_splits</a>]
    [&gt; filename]
    [&gt;&gt; filename]</code></pre>

The `report_checks` command reports paths in the design. Paths are reported in groups by capture clock, unclocked path delays, gated clocks and unconstrained.

See `set_false_path` for a description of allowed from_list, through_list and to_list objects.

### Options

`-from` {: #opt-report_checks-from }
: Report paths from a list of clocks, instances, ports, register clock pins, or latch data pins.

`-rise_from` {: #opt-report_checks-rise_from }
: Report  paths from the rising edge of clocks, instances, ports, register clock pins, or latch data pins.

`-fall_from` {: #opt-report_checks-fall_from }
: Report paths from the falling edge of clocks, instances, ports, register clock pins, or latch data pins.

`-through` {: #opt-report_checks-through }
: Report paths through a list of instances, pins or nets.

`-rise_through` {: #opt-report_checks-rise_through }
: Report rising paths through a list of instances, pins or nets.

`-fall_through` {: #opt-report_checks-fall_through }
: Report falling paths through a list of instances, pins or nets.

`-to` {: #opt-report_checks-to }
: Report paths to a list of clocks, instances, ports or pins.

`-rise_to` {: #opt-report_checks-rise_to }
: Report rising paths to a list of clocks, instances, ports or pins.

`-fall_to` {: #opt-report_checks-fall_to }
: Report falling paths to a list of clocks, instances, ports or pins.

`-unconstrained` {: #opt-report_checks-unconstrained }
: Report unconstrained paths also. The unconstrained path group is not reported without this option.

`-path_delay` {: #opt-report_checks-path_delay }
: - `min`: Report min path (hold) checks.
  - `min_rise`: Report min path (hold) checks for rising endpoints.
  - `min_fall`: Report min path (hold) checks for falling endpoints.
  - `max`: Report max path (setup) checks.
  - `max_rise`: Report max path (setup) checks for rising endpoints.
  - `max_fall`: Report max path (setup) checks for falling endpoints.
  - `min_max`: Report min and max path (setup and hold) checks.

`-scenes` {: #opt-report_checks-scenes }
: Report paths for these scenes. The default is all scenes.

`-group_path_count` {: #opt-report_checks-group_path_count }
: `path_count`: The number of paths to report in each path group. The default is 1.

`-endpoint_path_count` {: #opt-report_checks-endpoint_path_count }
: `endpoint_path_count`: The number of paths to report for each endpoint. The default is 1.

`-unique_paths_to_endpoint` {: #opt-report_checks-unique_paths_to_endpoint }
: When multiple paths to an endpoint are specified with `-endpoint_path_count`, many of the paths may differ only in the rise/fall edges of the pins in the paths. With this option only the worst path through the set of pins is reported.

`-unique_edges_to_endpoint` {: #opt-report_checks-unique_edges_to_endpoint }
: When multiple paths to an endpoint are specified with `-endpoint_path_count`, conditional timing arcs result in paths that go through the same pins and rise/fall edges. With this option only the worst path through the set of pins and rise/fall edges is reported.

`-slack_max` {: #opt-report_checks-slack_max }
: Only report paths with less slack than max_slack.

`-slack_min` {: #opt-report_checks-slack_min }
: Only report paths with more slack than min_slack.

`-sort_by_slack` {: #opt-report_checks-sort_by_slack }
: Sort paths by slack rather than slack grouped by path group.

`-path_group` {: #opt-report_checks-path_group }
: List of path groups to report. The default is to report all path groups.

`-format` {: #opt-report_checks-format }
: - `end`: Report path ends in one line with delay, required time and slack.
  - `full`: Report path start and end points and the path. This is the default path type.
  - `full_clock`: Report path start and end points, the path, and the source and target clock paths.
  - `full_clock_expanded`: Report path start and end points, the path, and the source and target clock paths. If the clock is generated and propagated, the path from the clock source pin is also reported.
  - `short`: Report only path start and end points.
  - `summary`: Report only path ends with delay.
  - `json`: Report in json format. `-fields` is ignored.

`-fields` {: #opt-report_checks-fields }
: List of capacitance|slew|input_pins|hierarchical_pins|net|fanout|src_attr|variation

`-digits` {: #opt-report_checks-digits }
: Number of digits to print after the decimal point.

`-no_line_splits` {: #opt-report_checks-no_line_splits }
: Do not split long lines into multiple lines.

## report_clock_latency

<pre><code>report_clock_latency
    [<a href="#opt-report_clock_latency-clocks">-clocks</a> clocks]
    [<a href="#opt-report_clock_latency-scenes">-scenes</a> scene]
    [<a href="#opt-report_clock_latency-include_internal_latency">-include_internal_latency</a>]
    [<a href="#opt-report_clock_latency-digits">-digits</a> digits]</code></pre>

Report the clock network latency.

### Options

`-clocks` {: #opt-report_clock_latency-clocks }
: The clocks to report. The default is all clocks.

`-scenes` {: #opt-report_clock_latency-scenes }
: Report latency for these scenes. The default is all scenes.

`-include_internal_latency` {: #opt-report_clock_latency-include_internal_latency }
: Include internal clock latency from liberty min/max_clock_tree_path timing groups.

`-digits` {: #opt-report_clock_latency-digits }
: Number of digits to print after the decimal point.

## report_clock_min_period

<pre><code>report_clock_min_period
    [<a href="#opt-report_clock_min_period-clocks">-clocks</a> clocks]
    [<a href="#opt-report_clock_min_period-include_port_paths">-include_port_paths</a>]</code></pre>

Report the minimum period and maximum frequency for clocks. If the `-clocks` argument is not specified all clocks are reported. The minimum period is determined by examining the smallest slack paths between registers on the rising edges of the clock or between falling edges of the clock. Paths between different clocks, different clock edges of the same clock, level-sensitive latches, or paths constrained by `set_multicycle_path` or `set_max_delay` are not considered.

### Options

`-clocks` {: #opt-report_clock_min_period-clocks }
: The clocks to report.

`-include_port_paths` {: #opt-report_clock_min_period-include_port_paths }
: Include paths from input port and to output ports.

## report_clock_properties

<pre><code>report_clock_properties
    [clocks]</code></pre>

The `report_clock_properties` command reports the period and rise/fall edge times for each clock that has been defined.

## report_clock_skew

<pre><code>report_clock_skew
    [<a href="#opt-report_clock_skew-setup">-setup</a>|<a href="#opt-report_clock_skew-hold">-hold</a>]
    [<a href="#opt-report_clock_skew-clocks">-clocks</a> clocks]
    [<a href="#opt-report_clock_skew-scenes">-scenes</a> scenes]
    [<a href="#opt-report_clock_skew-include_internal_latency">-include_internal_latency</a>]
    [<a href="#opt-report_clock_skew-digits">-digits</a> digits]</code></pre>

Report the maximum difference in clock arrival between every source and target register that has a path between the source and target registers.

### Options

`-setup` {: #opt-report_clock_skew-setup }
: Apply to setup checks.

`-hold` {: #opt-report_clock_skew-hold }
: Apply to hold checks.

`-clocks` {: #opt-report_clock_skew-clocks }
: The clocks to report. The default is all clocks.

`-scenes` {: #opt-report_clock_skew-scenes }
: Report clocks for these scenes. The default is all scenes.

`-include_internal_latency` {: #opt-report_clock_skew-include_internal_latency }
: Include internal clock latency from liberty min/max_clock_tree_path timing groups.

`-digits` {: #opt-report_clock_skew-digits }
: Number of digits to print after the decimal point.

## report_dcalc

<pre><code>report_dcalc
    [<a href="#opt-report_dcalc-from">-from</a> from_pin]
    [<a href="#opt-report_dcalc-to">-to</a> to_pin]
    [<a href="#opt-report_dcalc-scene">-scene</a> scene]
    [<a href="#opt-report_dcalc-min">-min</a>]
    [<a href="#opt-report_dcalc-max">-max</a>]
    [<a href="#opt-report_dcalc-digits">-digits</a> digits]</code></pre>

The `report_dcalc` command shows how the delays between instance pins are calculated. It is useful for debugging problems with delay calculation.

### Options

`-from` {: #opt-report_dcalc-from }
: Report delay calculations for timing arcs from instance input pin from_pin.

`-to` {: #opt-report_dcalc-to }
: Report delay calculations for timing arcs to instance output pin to_pin.

`-scene` {: #opt-report_dcalc-scene }
: Report delay calculations for this scene. Required if more than one scene is defined.

`-min` {: #opt-report_dcalc-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-report_dcalc-max }
: Apply to maximum (setup) analysis.

`-digits` {: #opt-report_dcalc-digits }
: Number of digits to print after the decimal point.

## report_disabled_edges

<pre><code>report_disabled_edges</code></pre>

The `report_disabled_edges` command reports disabled timing arcs along with the reason they are disabled. Each disabled timing arc is reported as the instance name along with the from and to ports of the arc. The disable reason is shown next. Arcs that are disabled with `set_disable_timing` are reported with constraint as the reason. Arcs that are disabled by constants are reported with constant as the reason along with the constant instance pin and value. Arcs that are disabled to break combinational feedback loops are reported with loop as the reason.

```
> report_disabled_edges
u1 A B constant B=0
```

## report_edges

<pre><code>report_edges
    [<a href="#opt-report_edges-from">-from</a> from_pin]
    [<a href="#opt-report_edges-to">-to</a> to_pin]
    [<a href="#opt-report_edges-digits">-digits</a> digits]
    [<a href="#opt-report_edges-report_variance">-report_variance</a>]</code></pre>

Report the edges/timing arcs and their delays in the timing graph from/to/between pins.

### Options

`-from` {: #opt-report_edges-from }
: Report edges/timing arcs from pin from_pin.

`-to` {: #opt-report_edges-to }
: Report edges/timing arcs to pin to_pin.

`-digits` {: #opt-report_edges-digits }
: Number of digits to print after the decimal point.

`-report_variance` {: #opt-report_edges-report_variance }
: Include delay distribution variance in the report.

## report_instance

<pre><code>report_instance
    [<a href="#opt-report_instance-connections">-connections</a>]
    [<a href="#opt-report_instance-verbose">-verbose</a>]
    instance_path
    [&gt; filename]
    [&gt;&gt; filename]</code></pre>

Report information about an instance.

### Options

`-connections` {: #opt-report_instance-connections }
: Deprecated; connections are always reported.

`-verbose` {: #opt-report_instance-verbose }
: Deprecated; verbose output is always used.

## report_lib_cell

<pre><code>report_lib_cell
    cell_name
    [&gt; filename]
    [&gt;&gt; filename]</code></pre>

Describe the liberty library cell cell_name.

## report_net

<pre><code>report_net
    [<a href="#opt-report_net-scene">-scene</a> scene]
    [<a href="#opt-report_net-digits">-digits</a> digits]
    net_path
    [&gt; filename]
    [&gt;&gt; filename]</code></pre>

Report the connections and capacitance of a net.

### Options

`-scene` {: #opt-report_net-scene }
: Restrict the command to one scene.

`-digits` {: #opt-report_net-digits }
: Number of digits to print after the decimal point.

## report_object_full_names

<pre><code>report_object_full_names
    objects</code></pre>

The `report_object_full_names` command prints the hierarchical name of each object, sorted by full name.

## report_object_names

<pre><code>report_object_names
    objects</code></pre>

The `report_object_names` command prints the name of each object, sorted by name.

## report_parasitic_annotation

<pre><code>report_parasitic_annotation
    [<a href="#opt-report_parasitic_annotation-name">-name</a> spef_name]
    [<a href="#opt-report_parasitic_annotation-report_unannotated">-report_unannotated</a>]</code></pre>

Report SPEF parasitic annotation completeness.

### Options

`-name` {: #opt-report_parasitic_annotation-name }
: SPEF annotation name from `read_spef -name`.

`-report_unannotated` {: #opt-report_parasitic_annotation-report_unannotated }
: Report unannotated and partially annotated nets.

## report_power

<pre><code>report_power
    [<a href="#opt-report_power-instances">-instances</a> instances]
    [<a href="#opt-report_power-highest_power_instances">-highest_power_instances</a> count]
    [<a href="#opt-report_power-scene">-scene</a> scene]
    [<a href="#opt-report_power-digits">-digits</a> digits]
    [<a href="#opt-report_power-format">-format</a> format]
    [&gt; filename]
    [&gt;&gt; filename]</code></pre>

The `report_power` command uses static power analysis based on propagated or annotated pin activities in the circuit using Liberty power models. The internal, switching, leakage and total power are reported. Design power is reported separately for combinational, sequential, macro and pad groups. Power values are reported in watts.

The `read_vcd` or `read_saif` commands can be used to read activities from a file based on simulation. If no simulation activities are available, the `set_power_activity` command should be used to set the activity of input ports or pins in the design. The default input activity and duty for inputs are 0.1 and 0.5 respectively. The activities are propagated from annotated input ports or pins through gates and used in the power calculations.

```
Group                  Internal  Switching    Leakage      Total
                          Power      Power      Power      Power
----------------------------------------------------------------
Sequential             3.29e-06   3.41e-08   2.37e-07   3.56e-06  92.4%
Combinational          1.86e-07   3.31e-08   7.51e-08   2.94e-07   7.6%
Macro                  0.00e+00   0.00e+00   0.00e+00   0.00e+00   0.0%
Pad                    0.00e+00   0.00e+00   0.00e+00   0.00e+00   0.0%
---------------------------------------------------------------
Total                  3.48e-06   6.72e-08   3.12e-07   3.86e-06 100.0%
                          90.2%       1.7%       8.1%
```

### Options

`-instances` {: #opt-report_power-instances }
: `instances`: Report the power for each instance of instances. If the instance is hierarchical the total power for the instances inside the hierarchical instance is reported.

`-highest_power_instances` {: #opt-report_power-highest_power_instances }
: `count`: Report the power for the count highest power instances.

`-scene` {: #opt-report_power-scene }
: Restrict the command to one scene.

`-digits` {: #opt-report_power-digits }
: Number of digits to print after the decimal point.

`-format` {: #opt-report_power-format }
: - `text`: Print a text table (the default).
  - `json`: Print JSON.

## report_required

<pre><code>report_required
    [<a href="#opt-report_required-scene">-scene</a> scene]
    [<a href="#opt-report_required-report_variance">-report_variance</a>]
    [<a href="#opt-report_required-digits">-digits</a> digits]
    pin</code></pre>

The `report_required` command reports min/max rise/fall required times at a pin with respect to each clock.

### Options

`-scene` {: #opt-report_required-scene }
: Restrict the command to one scene.

`-report_variance` {: #opt-report_required-report_variance }
: Include delay distribution variance in the report.

`-digits` {: #opt-report_required-digits }
: Number of digits to print after the decimal point.

## report_slack

<pre><code>report_slack
    [<a href="#opt-report_slack-scene">-scene</a> scene]
    [<a href="#opt-report_slack-report_variance">-report_variance</a>]
    [<a href="#opt-report_slack-digits">-digits</a> digits]
    pin</code></pre>

The `report_slack` command reports min/max rise/fall slack at a pin with respect to each clock.

### Options

`-scene` {: #opt-report_slack-scene }
: Restrict the command to one scene.

`-report_variance` {: #opt-report_slack-report_variance }
: Include delay distribution variance in the report.

`-digits` {: #opt-report_slack-digits }
: Number of digits to print after the decimal point.

## report_slews

<pre><code>report_slews
    [<a href="#opt-report_slews-scenes">-scenes</a> scenes]
    [<a href="#opt-report_slews-digits">-digits</a> digits]
    [<a href="#opt-report_slews-report_variance">-report_variance</a>]
    pin</code></pre>

Report the slews at a pin.

### Options

`-scenes` {: #opt-report_slews-scenes }
: Report slews for these scenes. The default is all scenes.

`-digits` {: #opt-report_slews-digits }
: Number of digits to print after the decimal point.

`-report_variance` {: #opt-report_slews-report_variance }
: Report SSTA distribution parameters.

## report_tns

<pre><code>report_tns
    [<a href="#opt-report_tns-min">-min</a>]
    [<a href="#opt-report_tns-max">-max</a>]
    [<a href="#opt-report_tns-digits">-digits</a> digits]</code></pre>

Report the total negative slack.

### Options

`-min` {: #opt-report_tns-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-report_tns-max }
: Apply to maximum (setup) analysis.

`-digits` {: #opt-report_tns-digits }
: Number of digits to print after the decimal point.

## report_units

<pre><code>report_units</code></pre>

Report the units used for command arguments and reporting.

```
report_units
 time 1ns
 capacitance 1pF
 resistance 1kohm
 voltage 1v
 current 1A
 power 1pW
 distance 1um
```

## report_wns

<pre><code>report_wns
    [<a href="#opt-report_wns-min">-min</a>]
    [<a href="#opt-report_wns-max">-max</a>]
    [<a href="#opt-report_wns-digits">-digits</a> digits]</code></pre>

Report the worst negative slack. If the worst slack is positive, zero is reported.

### Options

`-min` {: #opt-report_wns-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-report_wns-max }
: Apply to maximum (setup) analysis.

`-digits` {: #opt-report_wns-digits }
: Number of digits to print after the decimal point.

## report_worst_slack

<pre><code>report_worst_slack
    [<a href="#opt-report_worst_slack-min">-min</a>]
    [<a href="#opt-report_worst_slack-max">-max</a>]
    [<a href="#opt-report_worst_slack-digits">-digits</a> digits]</code></pre>

Report the worst slack in the design.

### Options

`-min` {: #opt-report_worst_slack-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-report_worst_slack-max }
: Apply to maximum (setup) analysis.

`-digits` {: #opt-report_worst_slack-digits }
: Number of digits to print after the decimal point.

## set_assigned_check

<pre><code>set_assigned_check
    <a href="#opt-set_assigned_check-setup">-setup</a>|<a href="#opt-set_assigned_check-hold">-hold</a>|<a href="#opt-set_assigned_check-recovery">-recovery</a>|<a href="#opt-set_assigned_check-removal">-removal</a>
    [<a href="#opt-set_assigned_check-rise">-rise</a>]
    [<a href="#opt-set_assigned_check-fall">-fall</a>]
    [<a href="#opt-set_assigned_check-scene">-scene</a> scene]
    [<a href="#opt-set_assigned_check-min">-min</a>]
    [<a href="#opt-set_assigned_check-max">-max</a>]
    [<a href="#opt-set_assigned_check-from">-from</a> from_pins]
    [<a href="#opt-set_assigned_check-to">-to</a> to_pins]
    [<a href="#opt-set_assigned_check-clock">-clock</a> rise|fall]
    [<a href="#opt-set_assigned_check-cond">-cond</a> sdf_cond]
    check_value</code></pre>

The `set_assigned_check` command is used to annotate the timing checks between two pins on an instance. The annotated delay overrides the calculated delay. This command is an interactive way to back-annotate delays like an SDF file.

### Options

`-setup` {: #opt-set_assigned_check-setup }
: Apply to setup checks.

`-hold` {: #opt-set_assigned_check-hold }
: Apply to hold checks.

`-recovery` {: #opt-set_assigned_check-recovery }
: Annotate recovery timing checks.

`-removal` {: #opt-set_assigned_check-removal }
: Annotate removal timing checks.

`-rise` {: #opt-set_assigned_check-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_assigned_check-fall }
: Restrict the command to falling transitions.

`-scene` {: #opt-set_assigned_check-scene }
: The name of a scene. The `-scene` keyword is required if more than one scene  is defined.

`-min` {: #opt-set_assigned_check-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_assigned_check-max }
: Apply to maximum (setup) analysis.

`-from` {: #opt-set_assigned_check-from }
: A list of pins for the clock.

`-to` {: #opt-set_assigned_check-to }
: A list of pins for the data.

`-clock` {: #opt-set_assigned_check-clock }
: `rise|fall`: The timing check clock pin transition.

`-cond` {: #opt-set_assigned_check-cond }
: SDF condition string for the annotated check.

## set_assigned_delay

<pre><code>set_assigned_delay
    <a href="#opt-set_assigned_delay-cell">-cell</a>|<a href="#opt-set_assigned_delay-net">-net</a>
    [<a href="#opt-set_assigned_delay-rise">-rise</a>]
    [<a href="#opt-set_assigned_delay-fall">-fall</a>]
    [<a href="#opt-set_assigned_delay-scene">-scene</a> scene]
    [<a href="#opt-set_assigned_delay-min">-min</a>]
    [<a href="#opt-set_assigned_delay-max">-max</a>]
    [<a href="#opt-set_assigned_delay-from">-from</a> from_pins]
    [<a href="#opt-set_assigned_delay-to">-to</a> to_pins]
    delay</code></pre>

The `set_assigned_delay` command is used to annotate the delays between two pins on an instance or net. The annotated delay overrides the calculated delay. This command is an interactive way to back-annotate delays like an SDF file.

### Options

`-cell` {: #opt-set_assigned_delay-cell }
: Annotate the delays between two pins on an instance.

`-net` {: #opt-set_assigned_delay-net }
: Annotate the delays between two pins on a net.

`-rise` {: #opt-set_assigned_delay-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_assigned_delay-fall }
: Restrict the command to falling transitions.

`-scene` {: #opt-set_assigned_delay-scene }
: The name of a scene. The `-scene` keyword is required if more than one scene is defined.

`-min` {: #opt-set_assigned_delay-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_assigned_delay-max }
: Apply to maximum (setup) analysis.

`-from` {: #opt-set_assigned_delay-from }
: A list of pins.

`-to` {: #opt-set_assigned_delay-to }
: A list of pins.

## set_assigned_transition

<pre><code>set_assigned_transition
    [<a href="#opt-set_assigned_transition-rise">-rise</a>]
    [<a href="#opt-set_assigned_transition-fall">-fall</a>]
    [<a href="#opt-set_assigned_transition-scene">-scene</a> scene]
    [<a href="#opt-set_assigned_transition-min">-min</a>]
    [<a href="#opt-set_assigned_transition-max">-max</a>]
    slew
    pins</code></pre>

The `set_assigned_transition` command is used to annotate the transition time (slew) of a pin. The annotated transition time overrides the calculated transition time.

### Options

`-rise` {: #opt-set_assigned_transition-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_assigned_transition-fall }
: Restrict the command to falling transitions.

`-scene` {: #opt-set_assigned_transition-scene }
: Annotate delays for scene.

`-min` {: #opt-set_assigned_transition-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_assigned_transition-max }
: Apply to maximum (setup) analysis.

## set_case_analysis

<pre><code>set_case_analysis
    0|1|zero|one|rise|rising|fall|falling
    pins</code></pre>

The `set_case_analysis` command sets the signal on a port or pin to a constant logic value. No paths are propagated from constant pins. Constant values set with the `set_case_analysis` command are propagated through downstream gates.

Conditional timing arcs with mode groups are controlled by logic values on the instance pins.

## set_clock_gating_check

<pre><code>set_clock_gating_check
    [<a href="#opt-set_clock_gating_check-setup">-setup</a> setup_time]
    [<a href="#opt-set_clock_gating_check-hold">-hold</a> hold_time]
    [<a href="#opt-set_clock_gating_check-rise">-rise</a>]
    [<a href="#opt-set_clock_gating_check-fall">-fall</a>]
    [<a href="#opt-set_clock_gating_check-low">-low</a>]
    [<a href="#opt-set_clock_gating_check-high">-high</a>]
    [objects]</code></pre>

The `set_clock_gating_check` command is used to add setup or hold timing checks for data signals used to gate clocks.

If no objects are specified the setup/hold margin is global and applies to all clock gating circuits in the design. If neither of the `-rise` and `-fall` options are used the setup/hold margin applies to the rising and falling  edges of the clock gating signal.

Normally the library cell function is used to determine the active state of the clock. The clock is active high for AND/NAND functions and active low for OR/NOR functions. The `-high` and `-low` options are used to specify the active state of the clock for other cells, such as a MUX.

If multiple `set_clock_gating_check` commands apply to a clock gating instance he priority of the commands is shown below (highest to lowest priority).

```
clock enable pin
instance
clock pin
clock
global
```

### Options

`-setup` {: #opt-set_clock_gating_check-setup }
: Apply to setup checks.

`-hold` {: #opt-set_clock_gating_check-hold }
: Apply to hold checks.

`-rise` {: #opt-set_clock_gating_check-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_clock_gating_check-fall }
: Restrict the command to falling transitions.

`-low` {: #opt-set_clock_gating_check-low }
: The gating clock is active low (pin and instance objects only).

`-high` {: #opt-set_clock_gating_check-high }
: The gating clock is active high (pin and instance objects only).

## set_clock_groups

<pre><code>set_clock_groups
    [<a href="#opt-set_clock_groups-name">-name</a> name]
    [<a href="#opt-set_clock_groups-logically_exclusive">-logically_exclusive</a>]
    [<a href="#opt-set_clock_groups-physically_exclusive">-physically_exclusive</a>]
    [<a href="#opt-set_clock_groups-asynchronous">-asynchronous</a>]
    [<a href="#opt-set_clock_groups-allow_paths">-allow_paths</a>]
    [<a href="#opt-set_clock_groups-comment">-comment</a> comment]
    <a href="#opt-set_clock_groups-group">-group</a>
    clocks</code></pre>

The `set_clock_groups` command is used to define groups of clocks that interact with each other. Clocks in different groups do not interact and paths between them are not reported. Use a `-group` argument for each clock group.

### Options

`-name` {: #opt-set_clock_groups-name }
: `name`: The clock group name.

`-logically_exclusive` {: #opt-set_clock_groups-logically_exclusive }
: The clocks in different groups do not interact logically but can be physically present on the same chip. Paths between clock groups are considered for noise analysis.

`-physically_exclusive` {: #opt-set_clock_groups-physically_exclusive }
: The clocks in different groups cannot be present at the same time on a chip. Paths between clock groups are not considered for noise analysis.

`-asynchronous` {: #opt-set_clock_groups-asynchronous }
: The clock groups are asynchronous. Paths between clock groups are considered for noise analysis.

`-allow_paths` {: #opt-set_clock_groups-allow_paths }
: Allow paths between clock groups (do not mark them as false).

`-comment` {: #opt-set_clock_groups-comment }
: Comment string saved with the constraint.

`-group` {: #opt-set_clock_groups-group }
: A list of clocks in one group. Repeat `-group` for each group.

## set_clock_latency

<pre><code>set_clock_latency
    [<a href="#opt-set_clock_latency-source">-source</a>]
    [<a href="#opt-set_clock_latency-clock">-clock</a> clock]
    [<a href="#opt-set_clock_latency-rise">-rise</a>]
    [<a href="#opt-set_clock_latency-fall">-fall</a>]
    [<a href="#opt-set_clock_latency-min">-min</a>]
    [<a href="#opt-set_clock_latency-max">-max</a>]
    [<a href="#opt-set_clock_latency-early">-early</a>]
    [<a href="#opt-set_clock_latency-late">-late</a>]
    delay
    objects</code></pre>

The `set_clock_latency` command describes expected delays of the clock tree when analyzing a design using ideal clocks. Use the `-source` option to specify latency at the clock source, also known as insertion delay. Source latency is delay in the clock tree that is external to the design or a clock tree internal to an instance that implements a complex logic function.

`set_clock_latency` removes propagated clock properties for the clocks and pins objects.

### Options

`-source` {: #opt-set_clock_latency-source }
: The latency is at the clock source.

`-clock` {: #opt-set_clock_latency-clock }
: `clock`: If multiple clocks are defined at a pin this use this option to specify the latency for a specific clock.

`-rise` {: #opt-set_clock_latency-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_clock_latency-fall }
: Restrict the command to falling transitions.

`-min` {: #opt-set_clock_latency-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_clock_latency-max }
: Apply to maximum (setup) analysis.

`-early` {: #opt-set_clock_latency-early }
: Apply to early (min path) values.

`-late` {: #opt-set_clock_latency-late }
: Apply to late (max path) values.

## set_clock_sense

<pre><code>set_clock_sense
    [<a href="#opt-set_clock_sense-positive">-positive</a>]
    [<a href="#opt-set_clock_sense-negative">-negative</a>]
    [<a href="#opt-set_clock_sense-pulse">-pulse</a> pulse_type]
    [<a href="#opt-set_clock_sense-stop_propagation">-stop_propagation</a>]
    [<a href="#opt-set_clock_sense-clock">-clock</a> clocks]
    pins</code></pre>

The `set_clock_sense` command is deprecated as of SDC 2.1. Use `set_sense -type clock` instead.

### Options

`-positive` {: #opt-set_clock_sense-positive }
: The clock sense is positive unate.

`-negative` {: #opt-set_clock_sense-negative }
: The clock sense is negative unate.

`-pulse` {: #opt-set_clock_sense-pulse }
: Pulse type. Not supported.

`-stop_propagation` {: #opt-set_clock_sense-stop_propagation }
: Stop propagating clocks at pins.

`-clock` {: #opt-set_clock_sense-clock }
: A list of clocks to apply the sense.

## set_clock_transition

<pre><code>set_clock_transition
    [<a href="#opt-set_clock_transition-rise">-rise</a>]
    [<a href="#opt-set_clock_transition-fall">-fall</a>]
    [<a href="#opt-set_clock_transition-min">-min</a>]
    [<a href="#opt-set_clock_transition-max">-max</a>]
    transition
    clocks</code></pre>

The `set_clock_transition` command describes expected transition times of the clock tree when analyzing a design using ideal clocks.

### Options

`-rise` {: #opt-set_clock_transition-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_clock_transition-fall }
: Restrict the command to falling transitions.

`-min` {: #opt-set_clock_transition-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_clock_transition-max }
: Apply to maximum (setup) analysis.

## set_clock_uncertainty

<pre><code>set_clock_uncertainty
    [<a href="#opt-set_clock_uncertainty-from">-from</a>|<a href="#opt-set_clock_uncertainty-rise_from">-rise_from</a>|<a href="#opt-set_clock_uncertainty-fall_from">-fall_from</a> from_clock]
    [<a href="#opt-set_clock_uncertainty-to">-to</a>|<a href="#opt-set_clock_uncertainty-rise_to">-rise_to</a>|<a href="#opt-set_clock_uncertainty-fall_to">-fall_to</a> to_clock]
    [<a href="#opt-set_clock_uncertainty-rise">-rise</a>]
    [<a href="#opt-set_clock_uncertainty-fall">-fall</a>]
    [<a href="#opt-set_clock_uncertainty-setup">-setup</a>]
    [<a href="#opt-set_clock_uncertainty-hold">-hold</a>]
    uncertainty
    [objects]</code></pre>

The `set_clock_uncertainty` command specifies the uncertainty or jitter in a clock. The uncertainty for a clock can be specified on its source pin or port, or the clock itself.

```
set_clock_uncertainty .1 [get_clock clk1]
```

Inter-clock uncertainty between the source and target clocks of timing checks is specified with the `-from`|`-rise_from`|`-fall_from` and `-to`|`-rise_to`|`-fall_to` arguments .

```
set_clock_uncertainty -from [get_clock clk1] -to [get_clocks clk2] .1
```

The following commands are equivalent.

```
set_clock_uncertainty -from [get_clock clk1] -rise_to [get_clocks clk2] .1
set_clock_uncertainty -from [get_clock clk1] -to [get_clocks clk2] -rise .1
```

### Options

`-from` {: #opt-set_clock_uncertainty-from }
: `from_clock`: Inter-clock uncertainty source clock.

`-rise_from` {: #opt-set_clock_uncertainty-rise_from }
: Restrict `-from` to rising transitions.

`-fall_from` {: #opt-set_clock_uncertainty-fall_from }
: Restrict `-from` to falling transitions.

`-to` {: #opt-set_clock_uncertainty-to }
: `to_clock`: Inter-clock uncertainty target clock.

`-rise_to` {: #opt-set_clock_uncertainty-rise_to }
: Restrict `-to` to rising transitions.

`-fall_to` {: #opt-set_clock_uncertainty-fall_to }
: Restrict `-to` to falling transitions.

`-rise` {: #opt-set_clock_uncertainty-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_clock_uncertainty-fall }
: Restrict the command to falling transitions.

`-setup` {: #opt-set_clock_uncertainty-setup }
: Apply to setup checks.

`-hold` {: #opt-set_clock_uncertainty-hold }
: Apply to hold checks.

## set_cmd_units

<pre><code>set_cmd_units
    [<a href="#opt-set_cmd_units-capacitance">-capacitance</a> cap_unit]
    [<a href="#opt-set_cmd_units-resistance">-resistance</a> res_unit]
    [<a href="#opt-set_cmd_units-time">-time</a> time_unit]
    [<a href="#opt-set_cmd_units-voltage">-voltage</a> voltage_unit]
    [<a href="#opt-set_cmd_units-current">-current</a> current_unit]
    [<a href="#opt-set_cmd_units-power">-power</a> power_unit]
    [<a href="#opt-set_cmd_units-distance">-distance</a> distance_unit]</code></pre>

The `set_cmd_units` command is used to change the units used by the STA command interpreter when parsing commands and reporting results. The default units are the units specified in the first Liberty library file that is read.

Units are specified as a scale factor followed by a unit name. The scale factors are as follows.

```
M 1E+6
k 1E+3
m 1E-3
u 1E-6
n 1E-9
p 1E-12
f 1E-15
```

An example of the `set_units` command is shown below.

```
set_cmd_units -time ns -capacitance pF -current mA -voltage V
              -resistance kOhm -distance um
```

### Options

`-capacitance` {: #opt-set_cmd_units-capacitance }
: `cap_unit`: The capacitance scale factor followed by 'f'.

`-resistance` {: #opt-set_cmd_units-resistance }
: `res_unit`: The resistance scale factor followed by 'ohm'.

`-time` {: #opt-set_cmd_units-time }
: `time_unit`: The time scale factor followed by 's'.

`-voltage` {: #opt-set_cmd_units-voltage }
: `voltage_unit`: The voltage scale factor followed by 'v'.

`-current` {: #opt-set_cmd_units-current }
: `current_unit`: The current scale factor followed by 'A'.

`-power` {: #opt-set_cmd_units-power }
: `power_unit`: The power scale factor followed by 'w'.

`-distance` {: #opt-set_cmd_units-distance }
: `distance_unit`: The distance scale factor followed by 'm'.

## set_data_check

<pre><code>set_data_check
    [<a href="#opt-set_data_check-from">-from</a> from_pin]
    [<a href="#opt-set_data_check-rise_from">-rise_from</a> from_pin]
    [<a href="#opt-set_data_check-fall_from">-fall_from</a> from_pin]
    [<a href="#opt-set_data_check-to">-to</a> to_pin]
    [<a href="#opt-set_data_check-rise_to">-rise_to</a> to_pin]
    [<a href="#opt-set_data_check-fall_to">-fall_to</a> to_pin]
    [<a href="#opt-set_data_check-setup">-setup</a> | <a href="#opt-set_data_check-hold">-hold</a>]
    [<a href="#opt-set_data_check-clock">-clock</a> clock]
    margin</code></pre>

The `set_data_check` command is used to add a setup or hold timing check between two pins.

### Options

`-from` {: #opt-set_data_check-from }
: `from_pin`: A pin used as the timing check reference.

`-rise_from` {: #opt-set_data_check-rise_from }
: Restrict `-from` to rising transitions.

`-fall_from` {: #opt-set_data_check-fall_from }
: Restrict `-from` to falling transitions.

`-to` {: #opt-set_data_check-to }
: `to_pin`: A pin that the setup/hold check is applied to.

`-rise_to` {: #opt-set_data_check-rise_to }
: Restrict `-to` to rising transitions.

`-fall_to` {: #opt-set_data_check-fall_to }
: Restrict `-to` to falling transitions.

`-setup` {: #opt-set_data_check-setup }
: Apply to setup checks.

`-hold` {: #opt-set_data_check-hold }
: Apply to hold checks.

`-clock` {: #opt-set_data_check-clock }
: `clock`: The setup/hold check clock.

## set_disable_inferred_clock_gating

<pre><code>set_disable_inferred_clock_gating
    objects</code></pre>

The `set_disable_inferred_clock_gating` command disables clock gating checks on a clock gating instance, clock gating pin, or clock gating enable pin.

## set_disable_timing

<pre><code>set_disable_timing
    [<a href="#opt-set_disable_timing-from">-from</a> from_port]
    [<a href="#opt-set_disable_timing-to">-to</a> to_port]
    objects</code></pre>

The `set_disable_timing` command is used to disable paths though pins in the design. There are many different forms of the command depending on the objects specified in objects.

All timing paths though an instance are disabled when objects contains an instance. Timing checks in the instance are not disabled.

```
set_disable_timing u2
```

The `-from` and `-to` options can be used to restrict the disabled path to those from, to or between specific pins on the instance.

```
set_disable_timing -from A u2
set_disable_timing -to Z u2
set_disable_timing -from A -to Z u2
```

A list of top level ports or instance pins can also be disabled.

```
set_disable_timing u2/Z
set_disable_timing in1
```

Timing paths though all instances of a library cell in the design can be disabled by naming the cell using a hierarchy separator between the library and cell name. Paths from or to a cell port can be disabled with the `-from` and `-to` options or a port name after library and cell names.

```
set_disable_timing liberty1/snl_bufx2
set_disable_timing -from A liberty1/snl_bufx
set_disable_timing -to Z liberty1/snl_bufx
set_disable_timing liberty1/snl_bufx2/A
```

### Options

`-from` {: #opt-set_disable_timing-from }
: From pin of the disabled timing arc on an instance or cell.

`-to` {: #opt-set_disable_timing-to }
: To pin of the disabled timing arc on an instance or cell.

## set_drive

<pre><code>set_drive
    [<a href="#opt-set_drive-rise">-rise</a>]
    [<a href="#opt-set_drive-fall">-fall</a>]
    [<a href="#opt-set_drive-min">-min</a>]
    [<a href="#opt-set_drive-max">-max</a>]
    resistance
    ports</code></pre>

The `set_drive` command describes the resistance of an input port external driver.

### Options

`-rise` {: #opt-set_drive-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_drive-fall }
: Restrict the command to falling transitions.

`-min` {: #opt-set_drive-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_drive-max }
: Apply to maximum (setup) analysis.

## set_driving_cell

<pre><code>set_driving_cell
    [<a href="#opt-set_driving_cell-lib_cell">-lib_cell</a> cell]
    [<a href="#opt-set_driving_cell-library">-library</a> library]
    [<a href="#opt-set_driving_cell-rise">-rise</a>]
    [<a href="#opt-set_driving_cell-fall">-fall</a>]
    [<a href="#opt-set_driving_cell-min">-min</a>]
    [<a href="#opt-set_driving_cell-max">-max</a>]
    [<a href="#opt-set_driving_cell-pin">-pin</a> pin]
    [<a href="#opt-set_driving_cell-from_pin">-from_pin</a> from_pin]
    [<a href="#opt-set_driving_cell-input_transition_rise">-input_transition_rise</a> trans_rise]
    [<a href="#opt-set_driving_cell-input_transition_fall">-input_transition_fall</a> trans_fall]
    [<a href="#opt-set_driving_cell-multiply_by">-multiply_by</a> factor]
    [<a href="#opt-set_driving_cell-dont_scale">-dont_scale</a>]
    [<a href="#opt-set_driving_cell-no_design_rule">-no_design_rule</a>]
    ports</code></pre>

The `set_driving_cell` command describes an input port external driver.

### Options

`-lib_cell` {: #opt-set_driving_cell-lib_cell }
: `cell_name`: The driving cell.

`-library` {: #opt-set_driving_cell-library }
: `library`: The driving cell library.

`-rise` {: #opt-set_driving_cell-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_driving_cell-fall }
: Restrict the command to falling transitions.

`-min` {: #opt-set_driving_cell-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_driving_cell-max }
: Apply to maximum (setup) analysis.

`-pin` {: #opt-set_driving_cell-pin }
: `pin`: The output port of the driving cell.

`-from_pin` {: #opt-set_driving_cell-from_pin }
: `from_pin`: Use timing arcs from from_pin to the output pin.

`-input_transition_rise` {: #opt-set_driving_cell-input_transition_rise }
: `trans_rise`: The transition time for a rising input at from_pin.

`-input_transition_fall` {: #opt-set_driving_cell-input_transition_fall }
: `trans_fall`: The transition time for a falling input at from_pin.

`-multiply_by` {: #opt-set_driving_cell-multiply_by }
: Scale factor applied to the driving cell delay. Ignored.

`-dont_scale` {: #opt-set_driving_cell-dont_scale }
: Do not scale the driving cell delay. Ignored.

`-no_design_rule` {: #opt-set_driving_cell-no_design_rule }
: Do not apply driving cell design rules. Ignored.

## set_false_path

<pre><code>set_false_path
    [<a href="#opt-set_false_path-setup">-setup</a>]
    [<a href="#opt-set_false_path-hold">-hold</a>]
    [<a href="#opt-set_false_path-rise">-rise</a>]
    [<a href="#opt-set_false_path-fall">-fall</a>]
    [<a href="#opt-set_false_path-reset_path">-reset_path</a>]
    [<a href="#opt-set_false_path-comment">-comment</a> comment]
    [<a href="#opt-set_false_path-from">-from</a> from_list]
    [<a href="#opt-set_false_path-rise_from">-rise_from</a> from_list]
    [<a href="#opt-set_false_path-fall_from">-fall_from</a> from_list]
    [<a href="#opt-set_false_path-through">-through</a> through_list]
    [<a href="#opt-set_false_path-rise_through">-rise_through</a> through_list]
    [<a href="#opt-set_false_path-fall_through">-fall_through</a> through_list]
    [<a href="#opt-set_false_path-to">-to</a> to_list]
    [<a href="#opt-set_false_path-rise_to">-rise_to</a> to_list]
    [<a href="#opt-set_false_path-fall_to">-fall_to</a> to_list]</code></pre>

The `set_false_path` command disables timing along a path from, through and to a group of design objects.

Objects in from_list can be clocks, register/latch instances, or register/latch clock pins. The `-rise_from` and `-fall_from` keywords restrict the false paths to a specific clock edge.

Objects in through_list can be nets, instances, instance pins, or hierarchical pins,. The `-rise_through` and `-fall_through` keywords restrict the false paths to a specific path edge that traverses through the object.

Objects in to_list can be clocks, register/latch instances, or register/latch clock pins. The `-rise_to` and `-fall_to` keywords restrict the false paths to a specific transition at the path end.

### Options

`-setup` {: #opt-set_false_path-setup }
: Apply to setup checks.

`-hold` {: #opt-set_false_path-hold }
: Apply to hold checks.

`-rise` {: #opt-set_false_path-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_false_path-fall }
: Restrict the command to falling transitions.

`-reset_path` {: #opt-set_false_path-reset_path }
: Remove any matching `set_false_path`, `set_multicycle_path`, `set_max_delay`, `set_min_delay` exceptions first.

`-comment` {: #opt-set_false_path-comment }
: Comment string saved with the constraint.

`-from` {: #opt-set_false_path-from }
: A list of clocks, instances, ports or pins.

`-rise_from` {: #opt-set_false_path-rise_from }
: Restrict `-from` to rising transitions.

`-fall_from` {: #opt-set_false_path-fall_from }
: Restrict `-from` to falling transitions.

`-through` {: #opt-set_false_path-through }
: A list of instances, pins or nets.

`-rise_through` {: #opt-set_false_path-rise_through }
: Restrict `-through` to rising transitions.

`-fall_through` {: #opt-set_false_path-fall_through }
: Restrict `-through` to falling transitions.

`-to` {: #opt-set_false_path-to }
: A list of clocks, instances, ports or pins.

`-rise_to` {: #opt-set_false_path-rise_to }
: Restrict `-to` to rising transitions.

`-fall_to` {: #opt-set_false_path-fall_to }
: Restrict `-to` to falling transitions.

## set_fanout_load

<pre><code>set_fanout_load
    fanout
    ports</code></pre>

This command is ignored.

## set_hierarchy_separator

<pre><code>set_hierarchy_separator
    separator</code></pre>

Set the character used to separate names in a hierarchical instance, net or pin name. This separator is used by the command interpreter to read arguments and print results. The default separator is '/'.

## set_ideal_latency

<pre><code>set_ideal_latency
    [<a href="#opt-set_ideal_latency-rise">-rise</a>]
    [<a href="#opt-set_ideal_latency-fall">-fall</a>]
    [<a href="#opt-set_ideal_latency-min">-min</a>]
    [<a href="#opt-set_ideal_latency-max">-max</a>]
    delay
    objects</code></pre>

The `set_ideal_latency` command is parsed but ignored.

### Options

`-rise` {: #opt-set_ideal_latency-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_ideal_latency-fall }
: Restrict the command to falling transitions.

`-min` {: #opt-set_ideal_latency-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_ideal_latency-max }
: Apply to maximum (setup) analysis.

## set_ideal_network

<pre><code>set_ideal_network
    [<a href="#opt-set_ideal_network-no_propagation">-no_propagation</a>]
    objects</code></pre>

The `set_ideal_network` command is parsed but ignored.

### Options

`-no_propagation` {: #opt-set_ideal_network-no_propagation }
: Do not propagate the ideal network. Ignored.

## set_ideal_transition

<pre><code>set_ideal_transition
    [<a href="#opt-set_ideal_transition-rise">-rise</a>]
    [<a href="#opt-set_ideal_transition-fall">-fall</a>]
    [<a href="#opt-set_ideal_transition-min">-min</a>]
    [<a href="#opt-set_ideal_transition-max">-max</a>]
    transition_time
    objects</code></pre>

The `set_ideal_transition` command is parsed but ignored.

### Options

`-rise` {: #opt-set_ideal_transition-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_ideal_transition-fall }
: Restrict the command to falling transitions.

`-min` {: #opt-set_ideal_transition-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_ideal_transition-max }
: Apply to maximum (setup) analysis.

## set_input_delay

<pre><code>set_input_delay
    [<a href="#opt-set_input_delay-rise">-rise</a>]
    [<a href="#opt-set_input_delay-fall">-fall</a>]
    [<a href="#opt-set_input_delay-max">-max</a>]
    [<a href="#opt-set_input_delay-min">-min</a>]
    [<a href="#opt-set_input_delay-clock">-clock</a> clock]
    [<a href="#opt-set_input_delay-clock_fall">-clock_fall</a>]
    [<a href="#opt-set_input_delay-reference_pin">-reference_pin</a> ref_pin]
    [<a href="#opt-set_input_delay-source_latency_included">-source_latency_included</a>]
    [<a href="#opt-set_input_delay-network_latency_included">-network_latency_included</a>]
    [<a href="#opt-set_input_delay-add_delay">-add_delay</a>]
    delay
    port_pin_list</code></pre>

The `set_input_delay` command is used to specify the arrival time of an input signal.

The following command sets the min, max, rise and fall times on the in1 input port 1.0 time units after the rising edge of clk1.

```
set_input_delay -clock clk1 1.0 [get_ports in1]
```

Use multiple commands with the `-add_delay` option to specify separate arrival times for min, max, rise and fall times or multiple clocks. For example, the following specifies separate arrival times with respect to clocks clk1 and clk2.

```
set_input_delay -clock clk1 1.0 [get_ports in1]
set_input_delay -add_delay -clock clk2 2.0 [get_ports in1]
```

The `-reference_pin` option is used to specify an arrival time with respect to the arrival on a pin in the clock network. For propagated clocks, the input arrival time is relative to the clock arrival time at the reference pin (the clock source latency and network latency from the clock source to the reference pin). For ideal clocks, input arrival time is relative to the reference pin clock source latency. With the `-clock_fall` flag the arrival time is relative to the falling transition at the reference pin. If no clocks arrive at the reference pin the `set_input_delay` command is ignored. If no `-clock` is specified the arrival time is with respect to all clocks that arrive at the reference pin. The `-source_latency_included` and `-network_latency_included` options cannot be used with `-reference_pin`.

Paths from inputs that do not have an arrival time defined by `set_input_delay` are not reported. Set the `sta_input_port_default_clock` variable to 1 to report paths from inputs without a `set_input_delay`.

### Options

`-rise` {: #opt-set_input_delay-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_input_delay-fall }
: Restrict the command to falling transitions.

`-max` {: #opt-set_input_delay-max }
: Apply to maximum (setup) analysis.

`-min` {: #opt-set_input_delay-min }
: Apply to minimum (hold) analysis.

`-clock` {: #opt-set_input_delay-clock }
: `clock`: The arrival time is from clock.

`-clock_fall` {: #opt-set_input_delay-clock_fall }
: The arrival time is from the falling edge of clock.

`-reference_pin` {: #opt-set_input_delay-reference_pin }
: `ref_pin`: The arrival time is with respect to the clock that arrives at ref_pin.

`-source_latency_included` {: #opt-set_input_delay-source_latency_included }
: D no add the clock source latency (insertion delay) to the delay value.

`-network_latency_included` {: #opt-set_input_delay-network_latency_included }
: Do not add the clock latency to the delay value when the clock is ideal.

`-add_delay` {: #opt-set_input_delay-add_delay }
: Add this arrival to any existing arrivals.

## set_input_transition

<pre><code>set_input_transition
    [<a href="#opt-set_input_transition-rise">-rise</a>]
    [<a href="#opt-set_input_transition-fall">-fall</a>]
    [<a href="#opt-set_input_transition-min">-min</a>]
    [<a href="#opt-set_input_transition-max">-max</a>]
    transition
    ports</code></pre>

The `set_input_transition` command is used to specify the transition time (slew) of an input signal.

### Options

`-rise` {: #opt-set_input_transition-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_input_transition-fall }
: Restrict the command to falling transitions.

`-min` {: #opt-set_input_transition-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_input_transition-max }
: Apply to maximum (setup) analysis.

## set_level_shifter_strategy

<pre><code>set_level_shifter_strategy
    [<a href="#opt-set_level_shifter_strategy-rule">-rule</a> rule_type]</code></pre>

This command is parsed and ignored by timing analysis.

### Options

`-rule` {: #opt-set_level_shifter_strategy-rule }
: Level shifter rule. Ignored.

## set_level_shifter_threshold

<pre><code>set_level_shifter_threshold
    [<a href="#opt-set_level_shifter_threshold-voltage">-voltage</a> volt]</code></pre>

This command is parsed and ignored by timing analysis.

### Options

`-voltage` {: #opt-set_level_shifter_threshold-voltage }
: Voltage threshold. Ignored.

## set_load

<pre><code>set_load
    [<a href="#opt-set_load-rise">-rise</a>]
    [<a href="#opt-set_load-fall">-fall</a>]
    [<a href="#opt-set_load-max">-max</a>]
    [<a href="#opt-set_load-min">-min</a>]
    [<a href="#opt-set_load-subtract_pin_load">-subtract_pin_load</a>]
    [<a href="#opt-set_load-pin_load">-pin_load</a>]
    [<a href="#opt-set_load-wire_load">-wire_load</a>]
    capacitance
    objects</code></pre>

The `set_load` command annotates wire capacitance on a net or external capacitance on a port. There are four different uses for the `set_load` commanc:

```
set_load -wire_load port  external port wire capacitance
set_load -pin_load port   external port pin capacitance
set_load port             same as -pin_load
set_load net              net wire capacitance
```

External port capacitance can be annotated separately with the `-pin_load` and `-wire_load` options. Without the `-pin_load` and `-wire_load` options pin capacitance is annotated.

When annotating net wire capacitance with the `-subtract_pin_load` option the capacitance of all instance pins connected to the net is subtracted from capacitance. Setting the capacitance on a net overrides SPEF parasitics for delay calculation.

### Options

`-rise` {: #opt-set_load-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_load-fall }
: Restrict the command to falling transitions.

`-max` {: #opt-set_load-max }
: Apply to maximum (setup) analysis.

`-min` {: #opt-set_load-min }
: Apply to minimum (hold) analysis.

`-subtract_pin_load` {: #opt-set_load-subtract_pin_load }
: Subtract the capacitance of all instance pins connected to the net from capacitance (nets only). If the resulting capacitance is negative, zero is used. Pin capacitances are ignored by delay calculation when this option is used.

`-pin_load` {: #opt-set_load-pin_load }
: capacitance is external instance pin capacitance (ports only).

`-wire_load` {: #opt-set_load-wire_load }
: capacitance is external wire capacitance (ports only).

## set_logic_dc

<pre><code>set_logic_dc
    port_list</code></pre>

Set a port or pin to a constant unknown logic value. No paths are propagated from constant pins.

## set_logic_one

<pre><code>set_logic_one
    port_list</code></pre>

Set a port or pin to a constant logic one value. No paths are propagated from constant pins. Constant values set with the `set_logic_one` command are not propagated through downstream gates.

## set_logic_zero

<pre><code>set_logic_zero
    port_list</code></pre>

Set a port or pin to a constant logic zero value. No paths are propagated from constant pins. Constant values set with the `set_logic_zero` command are not propagated through downstream gates.

## set_max_area

<pre><code>set_max_area
    area</code></pre>

The `set_max_area` command is ignored during timing but is included in SDC files that are written.

## set_max_capacitance

<pre><code>set_max_capacitance
    cap
    objects</code></pre>

The `set_max_capacitance` command is ignored during timing but is included in SDC files that are written.

## set_max_delay

<pre><code>set_max_delay
    [<a href="#opt-set_max_delay-rise">-rise</a>]
    [<a href="#opt-set_max_delay-fall">-fall</a>]
    [<a href="#opt-set_max_delay-ignore_clock_latency">-ignore_clock_latency</a>]
    [<a href="#opt-set_max_delay-reset_path">-reset_path</a>]
    [<a href="#opt-set_max_delay-probe">-probe</a>]
    [<a href="#opt-set_max_delay-comment">-comment</a> comment]
    [<a href="#opt-set_max_delay-from">-from</a> from_list]
    [<a href="#opt-set_max_delay-rise_from">-rise_from</a> from_list]
    [<a href="#opt-set_max_delay-fall_from">-fall_from</a> from_list]
    [<a href="#opt-set_max_delay-through">-through</a> through_list]
    [<a href="#opt-set_max_delay-rise_through">-rise_through</a> through_list]
    [<a href="#opt-set_max_delay-fall_through">-fall_through</a> through_list]
    [<a href="#opt-set_max_delay-to">-to</a> to_list]
    [<a href="#opt-set_max_delay-rise_to">-rise_to</a> to_list]
    [<a href="#opt-set_max_delay-fall_to">-fall_to</a> to_list]
    delay</code></pre>

The `set_max_delay` command constrains the maximum delay through combinational logic paths. See `set_false_path` for a description of allowed from_list, through_list and to_list objects. If the to_list ends at a timing check the setup/hold time is included in the path delay.

When the `-ignore_clock_latency` option is used clock latency at the source and destination of the path delay is ignored. The constraint is reported in the default path group (**default**) rather than the clock path group when the path ends at a timing check.

### Options

`-rise` {: #opt-set_max_delay-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_max_delay-fall }
: Restrict the command to falling transitions.

`-ignore_clock_latency` {: #opt-set_max_delay-ignore_clock_latency }
: Ignore clock latency at the source and target registers.

`-reset_path` {: #opt-set_max_delay-reset_path }
: Remove any matching `set_false_path`, `set_multicycle_path`, `set_max_delay`, `set_min_delay` exceptions first.

`-probe` {: #opt-set_max_delay-probe }
: Do not break paths at internal pins (non startpoints).

`-comment` {: #opt-set_max_delay-comment }
: Comment string saved with the constraint.

`-from` {: #opt-set_max_delay-from }
: A list of clocks, instances, ports or pins.

`-rise_from` {: #opt-set_max_delay-rise_from }
: Restrict `-from` to rising transitions.

`-fall_from` {: #opt-set_max_delay-fall_from }
: Restrict `-from` to falling transitions.

`-through` {: #opt-set_max_delay-through }
: A list of instances, pins or nets.

`-rise_through` {: #opt-set_max_delay-rise_through }
: Restrict `-through` to rising transitions.

`-fall_through` {: #opt-set_max_delay-fall_through }
: Restrict `-through` to falling transitions.

`-to` {: #opt-set_max_delay-to }
: A list of clocks, instances, ports or pins.

`-rise_to` {: #opt-set_max_delay-rise_to }
: Restrict `-to` to rising transitions.

`-fall_to` {: #opt-set_max_delay-fall_to }
: Restrict `-to` to falling transitions.

## set_max_dynamic_power

<pre><code>set_max_dynamic_power
    power
    [unit]</code></pre>

The `set_max_dynamic_power` command is ignored during timing but is included in SDC files that are written.

## set_max_fanout

<pre><code>set_max_fanout
    fanout
    objects</code></pre>

The `set_max_fanout` command is ignored during timing but is included in SDC files that are written.

## set_max_leakage_power

<pre><code>set_max_leakage_power
    power
    [unit]</code></pre>

The `set_max_leakage_power` command is ignored during timing but is included in SDC files that are written.

## set_max_time_borrow

<pre><code>set_max_time_borrow
    limit
    objects</code></pre>

The `set_max_time_borrow` command specifies the maximum amount of time that latches can borrow. Time borrowing is the time that a data input to a transparent latch arrives after the latch opens.

## set_max_transition

<pre><code>set_max_transition
    [<a href="#opt-set_max_transition-clock_path">-clock_path</a>]
    [<a href="#opt-set_max_transition-data_path">-data_path</a>]
    [<a href="#opt-set_max_transition-rise">-rise</a>]
    [<a href="#opt-set_max_transition-fall">-fall</a>]
    slew
    objects</code></pre>

The `set_max_transition` command is specifies the maximum transition time (slew) design rule checked by the `report_check_types` `-max_transition` command.

If specified for a design, the default maximum transition is set for the design.

If specified for a clock, the maximum transition is applied to all pins in the clock domain. The `-clock_path` option restricts the maximum transition to clocks in clock paths. The `-data_path` option restricts the maximum transition to clocks data paths. The `-clock_path`, `-data_path`, `-rise` and `-fall` options only apply to clock objects.

### Options

`-clock_path` {: #opt-set_max_transition-clock_path }
: Set the  max slew for clock paths.

`-data_path` {: #opt-set_max_transition-data_path }
: Set the  max slew for data paths.

`-rise` {: #opt-set_max_transition-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_max_transition-fall }
: Restrict the command to falling transitions.

## set_min_capacitance

<pre><code>set_min_capacitance
    cap
    objects</code></pre>

The `set_min_capacitance` command is ignored during timing but is included in SDC files that are written.

## set_min_delay

<pre><code>set_min_delay
    [<a href="#opt-set_min_delay-rise">-rise</a>]
    [<a href="#opt-set_min_delay-fall">-fall</a>]
    [<a href="#opt-set_min_delay-ignore_clock_latency">-ignore_clock_latency</a>]
    [<a href="#opt-set_min_delay-reset_path">-reset_path</a>]
    [<a href="#opt-set_min_delay-probe">-probe</a>]
    [<a href="#opt-set_min_delay-comment">-comment</a> comment]
    [<a href="#opt-set_min_delay-from">-from</a> from_list]
    [<a href="#opt-set_min_delay-rise_from">-rise_from</a> from_list]
    [<a href="#opt-set_min_delay-fall_from">-fall_from</a> from_list]
    [<a href="#opt-set_min_delay-through">-through</a> through_list]
    [<a href="#opt-set_min_delay-rise_through">-rise_through</a> through_list]
    [<a href="#opt-set_min_delay-fall_through">-fall_through</a> through_list]
    [<a href="#opt-set_min_delay-to">-to</a> to_list]
    [<a href="#opt-set_min_delay-rise_to">-rise_to</a> to_list]
    [<a href="#opt-set_min_delay-fall_to">-fall_to</a> to_list]
    delay</code></pre>

The `set_min_delay` command constrains the minimum delay through combinational logic. See `set_false_path` for a description of allowed from_list, through_list and to_list objects. If the to_list ends at a timing check the setup/hold time is included in the path delay.

When the `-ignore_clock_latency` option is used clock latency at the source and destination of the path delay is ignored. The constraint is reported in the default path group (**default**) rather than the clock path group when the path ends at a timing check.

### Options

`-rise` {: #opt-set_min_delay-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_min_delay-fall }
: Restrict the command to falling transitions.

`-ignore_clock_latency` {: #opt-set_min_delay-ignore_clock_latency }
: Ignore clock latency at the source and target registers.

`-reset_path` {: #opt-set_min_delay-reset_path }
: Remove any matching `set_false_path`, `set_multicycle_path`, `set_max_delay`, `set_min_delay` exceptions first.

`-probe` {: #opt-set_min_delay-probe }
: Do not break paths at internal pins (non startpoints).

`-comment` {: #opt-set_min_delay-comment }
: Comment string saved with the constraint.

`-from` {: #opt-set_min_delay-from }
: A list of clocks, instances, ports or pins.

`-rise_from` {: #opt-set_min_delay-rise_from }
: Restrict `-from` to rising transitions.

`-fall_from` {: #opt-set_min_delay-fall_from }
: Restrict `-from` to falling transitions.

`-through` {: #opt-set_min_delay-through }
: A list of instances, pins or nets.

`-rise_through` {: #opt-set_min_delay-rise_through }
: Restrict `-through` to rising transitions.

`-fall_through` {: #opt-set_min_delay-fall_through }
: Restrict `-through` to falling transitions.

`-to` {: #opt-set_min_delay-to }
: A list of clocks, instances, ports or pins.

`-rise_to` {: #opt-set_min_delay-rise_to }
: Restrict `-to` to rising transitions.

`-fall_to` {: #opt-set_min_delay-fall_to }
: Restrict `-to` to falling transitions.

## set_min_pulse_width

<pre><code>set_min_pulse_width
    [<a href="#opt-set_min_pulse_width-low">-low</a>]
    [<a href="#opt-set_min_pulse_width-high">-high</a>]
    value
    [objects]</code></pre>

If `-low` and `-high` are not specified the minimum width applies to both high and low pulses.

### Options

`-low` {: #opt-set_min_pulse_width-low }
: Set the minimum low pulse width.

`-high` {: #opt-set_min_pulse_width-high }
: Set the minimum high pulse width.

## set_mode

<pre><code>set_mode
    mode_name</code></pre>

Set the mode for SDC commands in the Tcl interpreter. If mode `mode_name` does not exist, it is created. When modes are created the default mode is deleted.

## set_multicycle_path

<pre><code>set_multicycle_path
    [<a href="#opt-set_multicycle_path-setup">-setup</a>]
    [<a href="#opt-set_multicycle_path-hold">-hold</a>]
    [<a href="#opt-set_multicycle_path-rise">-rise</a>]
    [<a href="#opt-set_multicycle_path-fall">-fall</a>]
    [<a href="#opt-set_multicycle_path-start">-start</a>]
    [<a href="#opt-set_multicycle_path-end">-end</a>]
    [<a href="#opt-set_multicycle_path-reset_path">-reset_path</a>]
    [<a href="#opt-set_multicycle_path-comment">-comment</a> comment]
    [<a href="#opt-set_multicycle_path-from">-from</a> from_list]
    [<a href="#opt-set_multicycle_path-rise_from">-rise_from</a> from_list]
    [<a href="#opt-set_multicycle_path-fall_from">-fall_from</a> from_list]
    [<a href="#opt-set_multicycle_path-through">-through</a> through_list]
    [<a href="#opt-set_multicycle_path-rise_through">-rise_through</a> through_list]
    [<a href="#opt-set_multicycle_path-fall_through">-fall_through</a> through_list]
    [<a href="#opt-set_multicycle_path-to">-to</a> to_list]
    [<a href="#opt-set_multicycle_path-rise_to">-rise_to</a> to_list]
    [<a href="#opt-set_multicycle_path-fall_to">-fall_to</a> to_list]
    path_multiplier</code></pre>

Normally the path between two registers or latches is assumed to take one clock cycle. The `set_multicycle_path` command overrides this assumption and allows multiple clock cycles for a timing check. See `set_false_path` for a description of allowed from_list, through_list and to_list objects.

### Options

`-setup` {: #opt-set_multicycle_path-setup }
: Apply to setup checks.

`-hold` {: #opt-set_multicycle_path-hold }
: Apply to hold checks.

`-rise` {: #opt-set_multicycle_path-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_multicycle_path-fall }
: Restrict the command to falling transitions.

`-start` {: #opt-set_multicycle_path-start }
: Multiply the source clock period by period_multiplier.

`-end` {: #opt-set_multicycle_path-end }
: Multiply the target clock period by period_multiplier.

`-reset_path` {: #opt-set_multicycle_path-reset_path }
: Remove any matching `set_false_path`, `set_multicycle_path`, `set_max_delay`, `set_min_delay` exceptions first.

`-comment` {: #opt-set_multicycle_path-comment }
: Comment string saved with the constraint.

`-from` {: #opt-set_multicycle_path-from }
: A list of clocks, instances, ports or pins.

`-rise_from` {: #opt-set_multicycle_path-rise_from }
: Restrict `-from` to rising transitions.

`-fall_from` {: #opt-set_multicycle_path-fall_from }
: Restrict `-from` to falling transitions.

`-through` {: #opt-set_multicycle_path-through }
: A list of instances, pins or nets.

`-rise_through` {: #opt-set_multicycle_path-rise_through }
: Restrict `-through` to rising transitions.

`-fall_through` {: #opt-set_multicycle_path-fall_through }
: Restrict `-through` to falling transitions.

`-to` {: #opt-set_multicycle_path-to }
: A list of clocks, instances, ports or pins.

`-rise_to` {: #opt-set_multicycle_path-rise_to }
: Restrict `-to` to rising transitions.

`-fall_to` {: #opt-set_multicycle_path-fall_to }
: Restrict `-to` to falling transitions.

## set_operating_conditions

<pre><code>set_operating_conditions
    [<a href="#opt-set_operating_conditions-analysis_type">-analysis_type</a> single|bc_wc|on_chip_variation]
    [<a href="#opt-set_operating_conditions-library">-library</a> lib]
    [condition]
    [<a href="#opt-set_operating_conditions-min">-min</a> min_condition]
    [<a href="#opt-set_operating_conditions-max">-max</a> max_condition]
    [<a href="#opt-set_operating_conditions-min_library">-min_library</a> min_lib]
    [<a href="#opt-set_operating_conditions-max_library">-max_library</a> max_lib]</code></pre>

The `set_operating_conditions` command is used to specify the type of analysis performed and the operating conditions used to derate library data.

### Options

`-analysis_type` {: #opt-set_operating_conditions-analysis_type }
: - `single`: Use one operating condition for min and max paths.
  - `bc_wc`: Best case, worst case analysis. Setup checks use max_condition for clock and data paths. Hold checks use the min_condition for clock and data paths.
  - `on_chip_variation`: The min and max operating conditions represent variations on the chip that can occur simultaneously. Setup checks use max_condition for data paths and    min_condition for clock paths. Hold checks use min_condition for data paths and max_condition for clock paths. This is the default analysis type.

`-library` {: #opt-set_operating_conditions-library }
: `lib`: The name of the library that contains condition.

`-min` {: #opt-set_operating_conditions-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_operating_conditions-max }
: Apply to maximum (setup) analysis.

`-min_library` {: #opt-set_operating_conditions-min_library }
: `min_lib`: The name of the library that contains min_condition.

`-max_library` {: #opt-set_operating_conditions-max_library }
: `max_lib`: The name of the library that contains max_condition.

## set_output_delay

<pre><code>set_output_delay
    [<a href="#opt-set_output_delay-rise">-rise</a>]
    [<a href="#opt-set_output_delay-fall">-fall</a>]
    [<a href="#opt-set_output_delay-max">-max</a>]
    [<a href="#opt-set_output_delay-min">-min</a>]
    [<a href="#opt-set_output_delay-clock">-clock</a> clock]
    [<a href="#opt-set_output_delay-clock_fall">-clock_fall</a>]
    [<a href="#opt-set_output_delay-reference_pin">-reference_pin</a> ref_pin]
    [<a href="#opt-set_output_delay-source_latency_included">-source_latency_included</a>]
    [<a href="#opt-set_output_delay-network_latency_included">-network_latency_included</a>]
    [<a href="#opt-set_output_delay-add_delay">-add_delay</a>]
    delay
    port_pin_list</code></pre>

The `set_output_delay` command is used to specify the external delay to a setup/hold check on an output port or internal pin that is clocked by clock. Unless the `-add_delay` option is specified any existing output delays are replaced.

The `-reference_pin` option is used to specify a timing check with respect to the arrival on a pin in the clock network. For propagated clocks, the timing check is relative to the clock arrival time at the reference pin (the clock source latency and network latency from the clock source to the reference pin). For ideal clocks, the timing check is relative to the reference pin clock source latency. With the `-clock_fall` flag the timing check is relative to the falling edge of the reference pin. If no clocks arrive at the reference pin the `set_output_delay` command is ignored. If no `-clock` is specified the timing check is with respect to all clocks that arrive at the reference pin. The `-source_latency_included` and `-network_latency_included` options cannot be used with `-reference_pin`.

### Options

`-rise` {: #opt-set_output_delay-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_output_delay-fall }
: Restrict the command to falling transitions.

`-max` {: #opt-set_output_delay-max }
: Apply to maximum (setup) analysis.

`-min` {: #opt-set_output_delay-min }
: Apply to minimum (hold) analysis.

`-clock` {: #opt-set_output_delay-clock }
: `clock`: The external check is to clock. The default clock edge is rising.

`-clock_fall` {: #opt-set_output_delay-clock_fall }
: The external check is to the falling edge of clock.

`-reference_pin` {: #opt-set_output_delay-reference_pin }
: `ref_pin`: The external check is clocked by the clock that arrives at ref_pin.

`-source_latency_included` {: #opt-set_output_delay-source_latency_included }
: Do not add the clock source latency (insertion delay) to the delay value.

`-network_latency_included` {: #opt-set_output_delay-network_latency_included }
: Do not add the clock latency to the delay value when the clock is ideal.

`-add_delay` {: #opt-set_output_delay-add_delay }
: Add this output delay to any existing output delays.

## set_path_margin

<pre><code>set_path_margin
    [<a href="#opt-set_path_margin-setup">-setup</a>]
    [<a href="#opt-set_path_margin-hold">-hold</a>]
    [<a href="#opt-set_path_margin-rise">-rise</a>]
    [<a href="#opt-set_path_margin-fall">-fall</a>]
    [<a href="#opt-set_path_margin-comment">-comment</a> comment]
    [<a href="#opt-set_path_margin-from">-from</a> from_list]
    [<a href="#opt-set_path_margin-rise_from">-rise_from</a> from_list]
    [<a href="#opt-set_path_margin-fall_from">-fall_from</a> from_list]
    [<a href="#opt-set_path_margin-through">-through</a>|<a href="#opt-set_path_margin-through">-thr</a>|<a href="#opt-set_path_margin-through">-th</a> through_list]
    [<a href="#opt-set_path_margin-rise_through">-rise_through</a>|<a href="#opt-set_path_margin-rise_through">-rise_thr</a>|<a href="#opt-set_path_margin-rise_through">-rise_th</a> through_list]
    [<a href="#opt-set_path_margin-fall_through">-fall_through</a>|<a href="#opt-set_path_margin-fall_through">-fall_thr</a>|<a href="#opt-set_path_margin-fall_through">-fall_th</a> through_list]
    [<a href="#opt-set_path_margin-to">-to</a> to_list]
    [<a href="#opt-set_path_margin-rise_to">-rise_to</a> to_list]
    [<a href="#opt-set_path_margin-fall_to">-fall_to</a> to_list]
    margin</code></pre>

The `set_path_margin` command applies a signed slack adjustment to matching timing paths on the capture-clock side. A positive margin makes the path harder to meet and a negative margin makes it easier. If neither `-setup` nor `-hold` is specified the margin applies to both. See `set_false_path` for a description of allowed from_list, through_list and to_list objects. At least one of `-from`, `-through`, or `-to` is required. Matching exceptions are removed with `unset_path_exceptions`.

### Options

`-through` {: #opt-set_path_margin-through }
: A list of instances, pins or nets.

`-rise_through` {: #opt-set_path_margin-rise_through }
: Restrict `-through` to rising transitions.

`-fall_through` {: #opt-set_path_margin-fall_through }
: Restrict `-through` to falling transitions.

`-setup` {: #opt-set_path_margin-setup }
: Apply to setup checks.

`-hold` {: #opt-set_path_margin-hold }
: Apply to hold checks.

`-rise` {: #opt-set_path_margin-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_path_margin-fall }
: Restrict the command to falling transitions.

`-comment` {: #opt-set_path_margin-comment }
: Comment string saved with the constraint.

`-from` {: #opt-set_path_margin-from }
: A list of clocks, instances, ports or pins.

`-rise_from` {: #opt-set_path_margin-rise_from }
: Restrict `-from` to rising transitions.

`-fall_from` {: #opt-set_path_margin-fall_from }
: Restrict `-from` to falling transitions.

`-to` {: #opt-set_path_margin-to }
: A list of clocks, instances, ports or pins.

`-rise_to` {: #opt-set_path_margin-rise_to }
: Restrict `-to` to rising transitions.

`-fall_to` {: #opt-set_path_margin-fall_to }
: Restrict `-to` to falling transitions.

## set_port_fanout_number

<pre><code>set_port_fanout_number
    [<a href="#opt-set_port_fanout_number-max">-max</a>]
    [<a href="#opt-set_port_fanout_number-min">-min</a>]
    fanout
    ports</code></pre>

Set the external fanout for ports.

### Options

`-max` {: #opt-set_port_fanout_number-max }
: Apply to maximum (setup) analysis.

`-min` {: #opt-set_port_fanout_number-min }
: Apply to minimum (hold) analysis.

## set_power_activity

<pre><code>set_power_activity
    [<a href="#opt-set_power_activity-global">-global</a>]
    [<a href="#opt-set_power_activity-input">-input</a>]
    [<a href="#opt-set_power_activity-input_ports">-input_ports</a> ports]
    [<a href="#opt-set_power_activity-pins">-pins</a> pins]
    [<a href="#opt-set_power_activity-activity">-activity</a> activity | <a href="#opt-set_power_activity-density">-density</a> density]
    [<a href="#opt-set_power_activity-duty">-duty</a> duty]
    [<a href="#opt-set_power_activity-clock">-clock</a> clock]</code></pre>

The `set_power_activity` command is used to set the activity and duty used for power analysis globally or for input ports or pins in the design.

The default input activity for inputs is 0.1 transitions per minimum clock period if a clock is defined or 0.0 if there are no clocks defined. The default input duty is 0.5. This is equivalent to the following command:

```
set_power_activity -input -activity 0.1 -duty 0.5
```

### Options

`-global` {: #opt-set_power_activity-global }
: Set the activity/duty for all non-clock pins.

`-input` {: #opt-set_power_activity-input }
: Set the default input port activity/duty.

`-input_ports` {: #opt-set_power_activity-input_ports }
: `input_ports`: Set the input port activity/duty.

`-pins` {: #opt-set_power_activity-pins }
: `pins`: Set the pin activity/duty.

`-activity` {: #opt-set_power_activity-activity }
: `activity`: The activity, or number of transitions per clock cycle. If clock is not specified the clock with the minimum period is used. If no clocks are defined an error is reported.

`-density` {: #opt-set_power_activity-density }
: `density`: Transitions per library time unit.

`-duty` {: #opt-set_power_activity-duty }
: `duty`: The duty, or probability the signal is high (0 <= duty <= 1.0). Defaults to 0.5.

`-clock` {: #opt-set_power_activity-clock }
: `clock`: The clock to use for the period with `-activity`. This option is ignored if `-density` is used.

## set_propagated_clock

<pre><code>set_propagated_clock
    objects</code></pre>

The `set_propagated_clock` command changes a clock tree from an ideal network that has no delay one that uses calculated or back-annotated gate and interconnect delays. When objects is a port or pin, clock delays downstream of the object are used.

## set_property

<pre><code>set_property
    object
    property
    value</code></pre>

The `set_property` command sets a user property defined with `define_property` on an object. Use `get_property` to read the value.

## set_pvt

<pre><code>set_pvt
    insts
    [<a href="#opt-set_pvt-min">-min</a>]
    [<a href="#opt-set_pvt-max">-max</a>]
    [<a href="#opt-set_pvt-process">-process</a> process]
    [<a href="#opt-set_pvt-voltage">-voltage</a> voltage]
    [<a href="#opt-set_pvt-temperature">-temperature</a> temperature]</code></pre>

The `set_pvt` command sets the process, voltage and temperature values used during delay calculation for a specific instance in the design.

### Options

`-min` {: #opt-set_pvt-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_pvt-max }
: Apply to maximum (setup) analysis.

`-process` {: #opt-set_pvt-process }
: `process`: A process value (float).

`-voltage` {: #opt-set_pvt-voltage }
: `voltage`: A voltage value (float).

`-temperature` {: #opt-set_pvt-temperature }
: `temperature`: A temperature value (float).

## set_resistance

<pre><code>set_resistance
    [<a href="#opt-set_resistance-min">-min</a>]
    [<a href="#opt-set_resistance-max">-max</a>]
    resistance
    nets</code></pre>

Set the resistance of nets.

### Options

`-min` {: #opt-set_resistance-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_resistance-max }
: Apply to maximum (setup) analysis.

## set_scene

<pre><code>set_scene
    scene_name</code></pre>

The `set_scene` command sets the scene used by subsequent commands. Use `get_scenes` to find defined scenes.

## set_sense

<pre><code>set_sense
    [<a href="#opt-set_sense-type">-type</a> clock|data]
    [<a href="#opt-set_sense-positive">-positive</a>]
    [<a href="#opt-set_sense-negative">-negative</a>]
    [<a href="#opt-set_sense-pulse">-pulse</a> pulse_type]
    [<a href="#opt-set_sense-stop_propagation">-stop_propagation</a>]
    [<a href="#opt-set_sense-clocks">-clocks</a> clocks]
    pins</code></pre>

The `set_sense` command is used to modify the propagation of a clock signal. The clock sense is set with the `-positive` and `-negative` flags. Use the `-stop_propagation` flag to stop the clock from propagating beyond a pin. The `-positive`, `-negative`, `-stop_propagation`, and `-pulse` options are mutually exclusive. If the `-clocks` option is not used the command applies to all clocks that traverse pins. The `-pulse` option is currently not supported.

### Options

`-type` {: #opt-set_sense-type }
: - `clock`: Set the sense for clock paths.
  - `data`: Set the sense for data paths (not supported).

`-positive` {: #opt-set_sense-positive }
: The clock sense is positive unate.

`-negative` {: #opt-set_sense-negative }
: The clock sense is negative unate.

`-pulse` {: #opt-set_sense-pulse }
: `pulse_type`: rise_triggered_high_pulse
  rise_triggered_low_pulse
  fall_triggered_high_pulse
  fall_triggered_low_pulse
  Not supported.

`-stop_propagation` {: #opt-set_sense-stop_propagation }
: Stop propagating clocks at pins.

`-clocks` {: #opt-set_sense-clocks }
: A list of clocks to apply the sense.

## set_timing_derate

<pre><code>set_timing_derate
    <a href="#opt-set_timing_derate-early">-early</a>|<a href="#opt-set_timing_derate-late">-late</a>
    [<a href="#opt-set_timing_derate-rise">-rise</a>]
    [<a href="#opt-set_timing_derate-fall">-fall</a>]
    [<a href="#opt-set_timing_derate-clock">-clock</a>]
    [<a href="#opt-set_timing_derate-data">-data</a>]
    [<a href="#opt-set_timing_derate-net_delay">-net_delay</a>]
    [<a href="#opt-set_timing_derate-cell_delay">-cell_delay</a>]
    [<a href="#opt-set_timing_derate-cell_check">-cell_check</a>]
    derate
    [objects]</code></pre>

The `set_timing_derate` command is used to derate delay calculation results used by the STA. If the `-early` and `-late` flags are omitted the both min and max paths are derated. If the `-clock` and `-data` flags are not used the derating both clock and data paths are derated.

Use the `unset_timing_derate` command to remove all derating factors.

### Options

`-early` {: #opt-set_timing_derate-early }
: Derate early (min) paths.

`-late` {: #opt-set_timing_derate-late }
: Derate late (max) paths.

`-rise` {: #opt-set_timing_derate-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-set_timing_derate-fall }
: Restrict the command to falling transitions.

`-clock` {: #opt-set_timing_derate-clock }
: Derate paths in the clock network.

`-data` {: #opt-set_timing_derate-data }
: Derate data paths.

`-net_delay` {: #opt-set_timing_derate-net_delay }
: Derate net (interconnect) delays.

`-cell_delay` {: #opt-set_timing_derate-cell_delay }
: Derate cell delays.

`-cell_check` {: #opt-set_timing_derate-cell_check }
: Derate cell timing check margins.

## set_units

<pre><code>set_units
    [<a href="#opt-set_units-time">-time</a> time_unit]
    [<a href="#opt-set_units-capacitance">-capacitance</a> cap_unit]
    [<a href="#opt-set_units-resistance">-resistance</a> res_unit]
    [<a href="#opt-set_units-voltage">-voltage</a> voltage_unit]
    [<a href="#opt-set_units-current">-current</a> current_unit]
    [<a href="#opt-set_units-power">-power</a> power_unit]
    [<a href="#opt-set_units-distance">-distance</a> distance_unit]</code></pre>

The `set_units` command is used to check the units used by the STA command interpreter when parsing commands and reporting results. If the current units differ from the set_unit value a warning is printed. Use the `set_cmd_units` command to change the command units.

Units are specified as a scale factor followed by a unit name. The scale factors are as follows.

M 1E+6
k 1E+3
m 1E-3
u 1E-6
n 1E-9
p 1E-12
f 1E-15

An example of the `set_units` command is shown below.

`set_units` `-time` ns `-capacitance` pF `-current` mA `-voltage` V `-resistance` kOhm

### Options

`-time` {: #opt-set_units-time }
: `time_unit`: The time scale factor followed by 's'.

`-capacitance` {: #opt-set_units-capacitance }
: `cap_unit`: The capacitance scale factor followed by 'f'.

`-resistance` {: #opt-set_units-resistance }
: `res_unit`: The resistance scale factor followed by 'ohm'.

`-voltage` {: #opt-set_units-voltage }
: `voltage_unit`: The voltage scale factor followed by 'v'.

`-current` {: #opt-set_units-current }
: `current_unit`: The current scale factor followed by 'A'.

`-power` {: #opt-set_units-power }
: `power_unit`: The power scale factor followed by 'w'.

`-distance` {: #opt-set_units-distance }
: `distance_unit`: The distance scale factor followed by 'm'.

## set_voltage

<pre><code>set_voltage
    [<a href="#opt-set_voltage-min">-min</a> min_case_value]
    [<a href="#opt-set_voltage-object_list">-object_list</a> power_nets]
    max_case_voltage</code></pre>

The `set_voltage` command sets the supply voltage used by SDC. The max-case voltage is always set globally. If `-object_list` is given, it is also set on those power nets.

### Options

`-min` {: #opt-set_voltage-min }
: Minimum (min delay) voltage. If omitted, only the max-case voltage is set.

`-object_list` {: #opt-set_voltage-object_list }
: Power nets to apply the voltage to.

## set_wire_load_min_block_size

<pre><code>set_wire_load_min_block_size
    block_size</code></pre>

The `set_wire_load_min_block_size` command is not supported.

## set_wire_load_mode

<pre><code>set_wire_load_mode
    top|enclosed|segmented</code></pre>

The `set_wire_load_mode` command is ignored during timing but is included in SDC files that are written.

## set_wire_load_model

<pre><code>set_wire_load_model
    <a href="#opt-set_wire_load_model-name">-name</a>
    model_name
    [<a href="#opt-set_wire_load_model-library">-library</a> lib_name]
    [<a href="#opt-set_wire_load_model-min">-min</a>]
    [<a href="#opt-set_wire_load_model-max">-max</a>]
    [objects]</code></pre>

Set the wire load model used to estimate net parasitics.

### Options

`-name` {: #opt-set_wire_load_model-name }
: `model_name`: The name of a wire load model.

`-library` {: #opt-set_wire_load_model-library }
: `library`: Library to look for model_name.

`-min` {: #opt-set_wire_load_model-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_wire_load_model-max }
: Apply to maximum (setup) analysis.

## set_wire_load_selection_group

<pre><code>set_wire_load_selection_group
    [<a href="#opt-set_wire_load_selection_group-library">-library</a> lib]
    [<a href="#opt-set_wire_load_selection_group-min">-min</a>]
    [<a href="#opt-set_wire_load_selection_group-max">-max</a>]
    group_name
    [objects]</code></pre>

The `set_wire_load_selection_group` command is parsed but not supported.

### Options

`-library` {: #opt-set_wire_load_selection_group-library }
: Library to look for group_name.

`-min` {: #opt-set_wire_load_selection_group-min }
: Apply to minimum (hold) analysis.

`-max` {: #opt-set_wire_load_selection_group-max }
: Apply to maximum (setup) analysis.

## suppress_msg

<pre><code>suppress_msg
    msg_ids</code></pre>

The `suppress_msg` command suppresses specified error/warning messages by ID. The list of message IDs can be found in `doc/Messages.md`.

## unset_case_analysis

<pre><code>unset_case_analysis
    pins</code></pre>

The `unset_case_analysis` command removes the constant values defined by the `set_case_analysis` command.

## unset_clock_groups

<pre><code>unset_clock_groups
    [<a href="#opt-unset_clock_groups-logically_exclusive">-logically_exclusive</a>]
    [<a href="#opt-unset_clock_groups-physically_exclusive">-physically_exclusive</a>]
    [<a href="#opt-unset_clock_groups-asynchronous">-asynchronous</a>]
    [<a href="#opt-unset_clock_groups-name">-name</a> names]
    [<a href="#opt-unset_clock_groups-all">-all</a>]</code></pre>

The `unset_clock_groups` command removes clock groups defined with `set_clock_groups`. One of `-logically_exclusive`, `-physically_exclusive`, or `-asynchronous` is required. Use `-all` to remove every group of that type, or `-name` to remove named groups.

### Options

`-logically_exclusive` {: #opt-unset_clock_groups-logically_exclusive }
: Remove logically exclusive clock groups.

`-physically_exclusive` {: #opt-unset_clock_groups-physically_exclusive }
: Remove physically exclusive clock groups.

`-asynchronous` {: #opt-unset_clock_groups-asynchronous }
: Remove asynchronous clock groups.

`-name` {: #opt-unset_clock_groups-name }
: Names of clock groups to remove.

`-all` {: #opt-unset_clock_groups-all }
: Remove all clock groups of the specified type.

## unset_clock_latency

<pre><code>unset_clock_latency
    [<a href="#opt-unset_clock_latency-source">-source</a>]
    [<a href="#opt-unset_clock_latency-clock">-clock</a> clock]
    objects</code></pre>

The `unset_clock_latency` command removes the clock latency set with the `set_clock_latency` command.

### Options

`-source` {: #opt-unset_clock_latency-source }
: Specifies source clock latency (clock insertion delay).

`-clock` {: #opt-unset_clock_latency-clock }
: If multiple clocks are defined at a pin, specify which clock latency to remove.

## unset_clock_transition

<pre><code>unset_clock_transition
    clocks</code></pre>

The `unset_clock_transition` command removes the clock transition set with the `set_clock_transition` command.

## unset_clock_uncertainty

<pre><code>unset_clock_uncertainty
    [<a href="#opt-unset_clock_uncertainty-from">-from</a>|<a href="#opt-unset_clock_uncertainty-rise_from">-rise_from</a>|<a href="#opt-unset_clock_uncertainty-fall_from">-fall_from</a> from_clock]
    [<a href="#opt-unset_clock_uncertainty-to">-to</a>|<a href="#opt-unset_clock_uncertainty-rise_to">-rise_to</a>|<a href="#opt-unset_clock_uncertainty-fall_to">-fall_to</a> to_clock]
    [<a href="#opt-unset_clock_uncertainty-rise">-rise</a>]
    [<a href="#opt-unset_clock_uncertainty-fall">-fall</a>]
    [<a href="#opt-unset_clock_uncertainty-setup">-setup</a>]
    [<a href="#opt-unset_clock_uncertainty-hold">-hold</a>]
    [objects]</code></pre>

The `unset_clock_uncertainty` command removes clock uncertainty defined with the `set_clock_uncertainty` command.

### Options

`-from` {: #opt-unset_clock_uncertainty-from }
: `from_clock`: Inter-clock uncertainty source clock.

`-rise_from` {: #opt-unset_clock_uncertainty-rise_from }
: Restrict `-from` to rising transitions.

`-fall_from` {: #opt-unset_clock_uncertainty-fall_from }
: Restrict `-from` to falling transitions.

`-to` {: #opt-unset_clock_uncertainty-to }
: `to_clock`: Inter-clock uncertainty target clock.

`-rise_to` {: #opt-unset_clock_uncertainty-rise_to }
: Restrict `-to` to rising transitions.

`-fall_to` {: #opt-unset_clock_uncertainty-fall_to }
: Restrict `-to` to falling transitions.

`-rise` {: #opt-unset_clock_uncertainty-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-unset_clock_uncertainty-fall }
: Restrict the command to falling transitions.

`-setup` {: #opt-unset_clock_uncertainty-setup }
: Apply to setup checks.

`-hold` {: #opt-unset_clock_uncertainty-hold }
: Apply to hold checks.

## unset_data_check

<pre><code>unset_data_check
    [<a href="#opt-unset_data_check-from">-from</a> from_pin]
    [<a href="#opt-unset_data_check-rise_from">-rise_from</a> from_pin]
    [<a href="#opt-unset_data_check-fall_from">-fall_from</a> from_pin]
    [<a href="#opt-unset_data_check-to">-to</a> to_pin]
    [<a href="#opt-unset_data_check-rise_to">-rise_to</a> to_pin]
    [<a href="#opt-unset_data_check-fall_to">-fall_to</a> to_pin]
    [<a href="#opt-unset_data_check-setup">-setup</a> | <a href="#opt-unset_data_check-hold">-hold</a>]
    [<a href="#opt-unset_data_check-clock">-clock</a> clock]</code></pre>

The `unset_clock_transition` command removes a setup or hold check defined by the `set_data_check` command.

### Options

`-from` {: #opt-unset_data_check-from }
: `from_object`: A pin used as the timing check reference.

`-rise_from` {: #opt-unset_data_check-rise_from }
: Restrict `-from` to rising transitions.

`-fall_from` {: #opt-unset_data_check-fall_from }
: Restrict `-from` to falling transitions.

`-to` {: #opt-unset_data_check-to }
: `to_object`: A pin that the setup/hold check is applied to.

`-rise_to` {: #opt-unset_data_check-rise_to }
: Restrict `-to` to rising transitions.

`-fall_to` {: #opt-unset_data_check-fall_to }
: Restrict `-to` to falling transitions.

`-setup` {: #opt-unset_data_check-setup }
: Apply to setup checks.

`-hold` {: #opt-unset_data_check-hold }
: Apply to hold checks.

`-clock` {: #opt-unset_data_check-clock }
: The setup/hold check clock.

## unset_disable_inferred_clock_gating

<pre><code>unset_disable_inferred_clock_gating
    objects</code></pre>

The `unset_disable_inferred_clock_gating` command removes a previous `set_disable_inferred_clock_gating` command.

## unset_disable_timing

<pre><code>unset_disable_timing
    [<a href="#opt-unset_disable_timing-from">-from</a> from_port]
    [<a href="#opt-unset_disable_timing-to">-to</a> to_port]
    objects</code></pre>

The `unset_disable_timing` command is used to remove the effect of previous  `set_disable_timing` commands.

### Options

`-from` {: #opt-unset_disable_timing-from }
: From pin of the disabled timing arc on an instance or cell.

`-to` {: #opt-unset_disable_timing-to }
: To pin of the disabled timing arc on an instance or cell.

## unset_input_delay

<pre><code>unset_input_delay
    [<a href="#opt-unset_input_delay-rise">-rise</a>]
    [<a href="#opt-unset_input_delay-fall">-fall</a>]
    [<a href="#opt-unset_input_delay-max">-max</a>]
    [<a href="#opt-unset_input_delay-min">-min</a>]
    [<a href="#opt-unset_input_delay-clock">-clock</a> clock]
    [<a href="#opt-unset_input_delay-clock_fall">-clock_fall</a>]
    port_pin_list</code></pre>

The `unset_input_delay` command removes a previously defined `set_input_delay`.

### Options

`-rise` {: #opt-unset_input_delay-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-unset_input_delay-fall }
: Restrict the command to falling transitions.

`-max` {: #opt-unset_input_delay-max }
: Apply to maximum (setup) analysis.

`-min` {: #opt-unset_input_delay-min }
: Apply to minimum (hold) analysis.

`-clock` {: #opt-unset_input_delay-clock }
: Unset the arrival time from clock.

`-clock_fall` {: #opt-unset_input_delay-clock_fall }
: Unset the arrival time from the falling edge of clock

## unset_output_delay

<pre><code>unset_output_delay
    [<a href="#opt-unset_output_delay-rise">-rise</a>]
    [<a href="#opt-unset_output_delay-fall">-fall</a>]
    [<a href="#opt-unset_output_delay-max">-max</a>]
    [<a href="#opt-unset_output_delay-min">-min</a>]
    [<a href="#opt-unset_output_delay-clock">-clock</a> clock]
    [<a href="#opt-unset_output_delay-clock_fall">-clock_fall</a>]
    port_pin_list</code></pre>

The `unset_output_delay` command a previously defined `set_output_delay`.

### Options

`-rise` {: #opt-unset_output_delay-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-unset_output_delay-fall }
: Restrict the command to falling transitions.

`-max` {: #opt-unset_output_delay-max }
: Apply to maximum (setup) analysis.

`-min` {: #opt-unset_output_delay-min }
: Apply to minimum (hold) analysis.

`-clock` {: #opt-unset_output_delay-clock }
: The arrival time is from this clock.

`-clock_fall` {: #opt-unset_output_delay-clock_fall }
: The arrival time is from the falling edge of clock

## unset_path_exceptions

<pre><code>unset_path_exceptions
    [<a href="#opt-unset_path_exceptions-setup">-setup</a>]
    [<a href="#opt-unset_path_exceptions-hold">-hold</a>]
    [<a href="#opt-unset_path_exceptions-rise">-rise</a>]
    [<a href="#opt-unset_path_exceptions-fall">-fall</a>]
    [<a href="#opt-unset_path_exceptions-from">-from</a> from_list]
    [<a href="#opt-unset_path_exceptions-rise_from">-rise_from</a> from_list]
    [<a href="#opt-unset_path_exceptions-fall_from">-fall_from</a> from_list]
    [<a href="#opt-unset_path_exceptions-through">-through</a> through_list]
    [<a href="#opt-unset_path_exceptions-rise_through">-rise_through</a> through_list]
    [<a href="#opt-unset_path_exceptions-fall_through">-fall_through</a> through_list]
    [<a href="#opt-unset_path_exceptions-to">-to</a> to_list]
    [<a href="#opt-unset_path_exceptions-rise_to">-rise_to</a> to_list]
    [<a href="#opt-unset_path_exceptions-fall_to">-fall_to</a> to_list]</code></pre>

The `unset_path_exceptions` command removes any matching `set_false_path`, `set_multicycle_path`, `set_max_delay`, `set_min_delay`, and `set_path_margin` exceptions.

### Options

`-setup` {: #opt-unset_path_exceptions-setup }
: Apply to setup checks.

`-hold` {: #opt-unset_path_exceptions-hold }
: Apply to hold checks.

`-rise` {: #opt-unset_path_exceptions-rise }
: Restrict the command to rising transitions.

`-fall` {: #opt-unset_path_exceptions-fall }
: Restrict the command to falling transitions.

`-from` {: #opt-unset_path_exceptions-from }
: `from`: A list of clocks, instances, ports or pins.

`-rise_from` {: #opt-unset_path_exceptions-rise_from }
: Restrict `-from` to rising transitions.

`-fall_from` {: #opt-unset_path_exceptions-fall_from }
: Restrict `-from` to falling transitions.

`-through` {: #opt-unset_path_exceptions-through }
: `through`: A list of instances, pins or nets.

`-rise_through` {: #opt-unset_path_exceptions-rise_through }
: Restrict `-through` to rising transitions.

`-fall_through` {: #opt-unset_path_exceptions-fall_through }
: Restrict `-through` to falling transitions.

`-to` {: #opt-unset_path_exceptions-to }
: `to`: A list of clocks, instances, ports or pins.

`-rise_to` {: #opt-unset_path_exceptions-rise_to }
: Restrict `-to` to rising transitions.

`-fall_to` {: #opt-unset_path_exceptions-fall_to }
: Restrict `-to` to falling transitions.

## unset_power_activity

<pre><code>unset_power_activity
    [<a href="#opt-unset_power_activity-global">-global</a>]
    [<a href="#opt-unset_power_activity-input">-input</a>]
    [<a href="#opt-unset_power_activity-input_ports">-input_ports</a> ports]
    [<a href="#opt-unset_power_activity-pins">-pins</a> pins]
    [<a href="#opt-unset_power_activity-clock">-clock</a> clock]</code></pre>

The unset_power_activity_command is used to undo the effects of the `set_power_activity` command.

### Options

`-global` {: #opt-unset_power_activity-global }
: Unset the activity/duty for all non-clock pins.

`-input` {: #opt-unset_power_activity-input }
: Unset the default input port activity/duty.

`-input_ports` {: #opt-unset_power_activity-input_ports }
: `input_ports`: Unset the input port activity/duty.

`-pins` {: #opt-unset_power_activity-pins }
: `pins`: Unset the pin activity/duty.

`-clock` {: #opt-unset_power_activity-clock }
: `clock`: Unset activity associated with this clock.

## unset_propagated_clock

<pre><code>unset_propagated_clock
    objects</code></pre>

Remove a previous `set_propagated_clock` command.

## unset_timing_derate

<pre><code>unset_timing_derate</code></pre>

Remove all derating factors set with the `set_timing_derate` command.

## unsuppress_msg

<pre><code>unsuppress_msg
    msg_ids</code></pre>

The `unsuppress_msg` command removes suppressions for the specified error/warning messages by ID. The list of message IDs can be found in `doc/Messages.md`.

## user_run_time

<pre><code>user_run_time</code></pre>

Returns the total user cpu run time in seconds as a float.

## with_output_to_variable

<pre><code>with_output_to_variable
    var
    { cmds }</code></pre>

The `with_output_to_variable` command redirects the output of Tcl commands to a variable.

## write_path_spice

<pre><code>write_path_spice
    <a href="#opt-write_path_spice-path_args">-path_args</a>
    path_args
    <a href="#opt-write_path_spice-spice_file">-spice_file</a>
    spice_file
    <a href="#opt-write_path_spice-lib_subckt_file">-lib_subckt_file</a>
    lib_subckts_file
    <a href="#opt-write_path_spice-model_file">-model_file</a>
    model_file
    <a href="#opt-write_path_spice-power">-power</a>
    power
    <a href="#opt-write_path_spice-ground">-ground</a>
    ground
    [<a href="#opt-write_path_spice-simulator">-simulator</a> hspice|ngspice|xyce]</code></pre>

The `write_path_spice` command writes a spice netlist for timing paths. Use path_args to specify `-from`/`-through`/`-to` as arguments to the `find_timing_paths` command. For each path, a spice netlist and the subckts referenced by the path are written in spice_directory. The spice netlist is written in path_<id>.sp and subckt file is path_<id>.subckt.

The spice netlists used by the path are written to subckt_file, which spice_file .includes. The device models used by the spice subckt netlists in model_file are also .included in spice_file. Power and ground names are specified with the `-power` and `-ground` arguments. The spice netlist includes a piecewise linear voltage source at the input and .measure statement for each gate delay and pin slew.

Example command:

```
write_path_spice -path_args {-from "in0" -to "out1" -unconstrained}  -spice_directory $result_dir  -lib_subckt_file "write_spice1.subckt"  -model_file "write_spice1.models"  -power VDD -ground VSS
```

When the simulator is hspice, .measure statements will be added to the spice netlist.

When the simulator is Xyce, the .print statement selects the CSV format and writes the waveform data to a file name path_<id>.csv so the results can be used by gnuplot.

### Options

`-path_args` {: #opt-write_path_spice-path_args }
: `-from`|`-through`|`-to` arguments as in `report_checks`.

`-spice_file` {: #opt-write_path_spice-spice_file }
: Directory and path prefix for spice output files.

`-lib_subckt_file` {: #opt-write_path_spice-lib_subckt_file }
: Cell transistor level subckts.

`-model_file` {: #opt-write_path_spice-model_file }
: Transistor model definitions .included by spice_file.

`-power` {: #opt-write_path_spice-power }
: Voltage supply name in voltage_map of the default liberty library.

`-ground` {: #opt-write_path_spice-ground }
: Ground supply name in voltage_map of the default liberty library.

`-simulator` {: #opt-write_path_spice-simulator }
: Simulator that will read the spice netlist.

## write_sdc

<pre><code>write_sdc
    [<a href="#opt-write_sdc-mode">-mode</a> mode]
    [<a href="#opt-write_sdc-map_hpins">-map_hpins</a>]
    [<a href="#opt-write_sdc-digits">-digits</a> digits]
    [<a href="#opt-write_sdc-gzip">-gzip</a>]
    [<a href="#opt-write_sdc-no_timestamp">-no_timestamp</a>]
    filename</code></pre>

Write the constraints for the design in SDC format to filename.

### Options

`-mode` {: #opt-write_sdc-mode }
: SDC mode to write. The default is the current mode.

`-map_hpins` {: #opt-write_sdc-map_hpins }
: Map hierarchical pins to leaf pins in the SDC.

`-digits` {: #opt-write_sdc-digits }
: Number of digits to print after the decimal point.

`-gzip` {: #opt-write_sdc-gzip }
: Compress the SDC with gzip.

`-no_timestamp` {: #opt-write_sdc-no_timestamp }
: Do not include a time and date in the SDC file.

## write_sdf

<pre><code>write_sdf
    [<a href="#opt-write_sdf-scene">-scene</a> scene]
    [<a href="#opt-write_sdf-divider">-divider</a> /|.]
    [<a href="#opt-write_sdf-include_typ">-include_typ</a>]
    [<a href="#opt-write_sdf-digits">-digits</a> digits]
    [<a href="#opt-write_sdf-gzip">-gzip</a>]
    [<a href="#opt-write_sdf-no_timestamp">-no_timestamp</a>]
    [<a href="#opt-write_sdf-no_version">-no_version</a>]
    filename</code></pre>

Write the delay calculation delays for the design in SDF format to `filename`. If `-scene` is not specified the min/max delays are across all scenes. With `-scene` the min/max delays for that scene are written. The SDF TIMESCALE is the same as the time_unit in the first Liberty file read.

### Options

`-scene` {: #opt-write_sdf-scene }
: Write delays for scene.

`-divider` {: #opt-write_sdf-divider }
: Divider to use between hierarchy levels in pin and instance names.

`-include_typ` {: #opt-write_sdf-include_typ }
: Include a 'typ' value in the SDF triple that is the average of min and max delays to satisfy some Verilog simulators that require three values in the delay triples.

`-digits` {: #opt-write_sdf-digits }
: Number of digits to print after the decimal point.

`-gzip` {: #opt-write_sdf-gzip }
: Compress the SDF using gzip.

`-no_timestamp` {: #opt-write_sdf-no_timestamp }
: Do not write a DATE statement.

`-no_version` {: #opt-write_sdf-no_version }
: Do not write a VERSION statement.

## write_timing_model

<pre><code>write_timing_model
    [<a href="#opt-write_timing_model-scene">-scene</a> scene]
    [<a href="#opt-write_timing_model-library_name">-library_name</a> lib_name]
    [<a href="#opt-write_timing_model-cell_name">-cell_name</a> cell_name]
    filename</code></pre>

The `write_timing_model` command constructs a liberty timing model for the current design and writes it to filename. cell_name defaults to the cell name of the top level block in the design.

The SDC used to extract the block should include the clock definitions. If the block contains a clock network `set_propagated_clock` should be used so the clock delays are included in the timing model. The following SDC commands are ignored when building the timing model.

```
set_input_delay
set_output_delay
set_load
set_timing_derate
```

Using `set_input_transition` with the slew from the block context will be used will improve the match between the timing model and the block netlist.  Paths defined on clocks that are defined on internal pins are ignored because the model has no way to include the clock definition.

The resulting timing model can be used in a hierarchical timing flow as a replacement for the block to speed up timing analysis. This hierarchical timing methodology does not handle timing exceptions that originate or terminate inside the block. The timing model includes:

```
combinational paths between inputs and outputs
setup and hold timing constraints on inputs
clock to output timing paths
```

Resistance of long wires on inputs and outputs of the block cannot be modeled in Liberty. To reduce inaccuracies from wire resistance in technologies with resistive wires place buffers on inputs and ouputs.

The extracted timing model setup/hold checks are scalar (no input slew dependence). Delay timing arcs are load dependent but do not include input slew dependency.

### Options

`-scene` {: #opt-write_timing_model-scene }
: The scene to use for extracting the model.

`-library_name` {: #opt-write_timing_model-library_name }
: The name to use for the liberty library. Defaults to cell_name.

`-cell_name` {: #opt-write_timing_model-cell_name }
: The name to use for the liberty cell. Defaults to the top level module name.

## write_verilog

<pre><code>write_verilog
    [<a href="#opt-write_verilog-include_pwr_gnd">-include_pwr_gnd</a>]
    [<a href="#opt-write_verilog-remove_cells">-remove_cells</a> cells]
    filename</code></pre>

The `write_verilog` command writes a Verilog netlist to filename. Use `-sort` to sort the instances so the results are reproducible across operating systems. Use `-remove_cells` to remove instances of lib_cells from the netlist.

### Options

`-include_pwr_gnd` {: #opt-write_verilog-include_pwr_gnd }
: Include power and ground pins on instances.

`-remove_cells` {: #opt-write_verilog-remove_cells }
: `lib_cells`: Liberty cells to remove from the Verilog netlist. Use `get_lib_cells`, a list of cells names, or a cell name with wildcards.

