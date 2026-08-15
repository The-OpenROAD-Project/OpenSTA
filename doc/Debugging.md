# Debugging timing

Here are some guidelines for debugging your design if static timing
does not report any paths, or does not report the expected paths.

Debugging timing problems generally involves using the following
commands to follow the propagation of arrival times from a known
arrival downstream to understand why the arrival times are not
propagating:

```tcl
report_edges
report_arrival
report_net
```

`report_edges -from` can be used to walk forward and `report_edges -to`
to walk backward in the netlist/timing graph. `report_arrival` shows
the min/max rise/fall arrival times with respect to each clock that has
a path to the pin. `report_net` shows connections to a net across
hierarchy levels.

## No paths found

The `report_checks` command only reports paths that are constrained by
timing checks or SDC commands such as `set_output_delay`. If the design
has only combinational logic (no registers or latches), there are no
timing checks, so no paths are reported. Use the `-unconstrained`
option to `report_checks` to see unconstrained paths.

```tcl
% report_checks -unconstrained
```

If the design is sequential (has registers or latches) and no paths are
reported, it is likely that there is a problem with the clock
propagation. Check the timing at a register in the design with the
`report_arrival` command.

```tcl
% report_arrival r1/CP
 (clk ^) r 0.00:0.00 f INF:-INF
 (clk v) r INF:-INF f 5.00:5.00
```

In this example the rising edge of the clock `clk` causes the rising
arrival min:max time at 0.00, and the falling edge arrives at 5.00.
Since the rising edge of the clock causes the rising edge of the
register clock pin, the clock path is positive unate.

The clock path should be positive or negative unate. Something is
probably wrong with the clock network if it is non-unate. A non-unate
clock path will report arrivals similar to the following:

```tcl
% report_arrival r1/CP
 (clk ^) r 0.00:0.00 f 0.00:0.00
 (clk v) r 5.00:5.00 f 5.00:5.00
```

Notice that each clock edge causes both rise and fall arrivals at the
register clock pin.

If there are no paths to the register clock pin, nothing is printed.
Use the `report_edges -to` command to find the gate driving the clock
pin.

```tcl
% report_edges -to r1/CP
i1/ZN -> CP wire
  ^ -> ^ 0.00:0.00
  v -> v 0.00:0.00
```

This shows that the gate/pin `i1/ZN` is driving the clock pin. The
`report_edges -to` command can be used to walk backward or forward
through the netlist one gate/net at a time. By checking the arrivals
with the `report_arrival` command you can determine where the path is
broken.

## No path reported at an endpoint

In order for a timing check to be reported, there must be an arrival
time at the data pin (the constrained pin) as well as the timing check
clock pin. If `report_checks -to` a register input does not report any
paths, check that the input is constrained by a timing check with
`report_edges -to`.

```tcl
% report_edges -to r1/D
CP -> D hold
  ^ -> ^ -0.04:-0.04
  ^ -> v -0.03:-0.03
CP -> D setup
  ^ -> ^ 0.09:0.09
  ^ -> v 0.08:0.08
in1 -> D wire
  ^ -> ^ 0.00:0.00
  v -> v 0.00:0.00
```

This reports the setup and hold checks for the D pin of `r1`.

Next, check the arrival times at the D and CP pins of the register with
`report_arrival`.

```tcl
% report_arrival r1/D
 (clk1 ^) r 1.00:1.00 f 1.00:1.00
% report_arrival r1/CP
 (clk1 ^) r 0.00:0.00 f INF:-INF
 (clk1 v) r INF:-INF f 5.00:5.00
```

If there are no arrivals on an input port of the design, use the
`set_input_delay` command to specify the arrival times on the port.
