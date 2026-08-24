Documention is publish at https://opensta.readthedocs.io/en/latest/ using
.readthedocs.yaml.

Command documention is declared in the various TCL command file such as search/Search.tcl.
An example is show below.

define_cmd_args "check_setup" \
  { [-verbose] [-no_input_delay] [-no_output_delay]\
      [-multiple_clock] [-no_clock]\
      [-unconstrained_endpoints] [-loops] [-generated_clocks]\
      [> filename] [>> filename] } \
  -help {The `check_setup` command performs sanity checks on the design. Individual checks can be performed with the keywords. If no check keywords are specified all checks are performed. Checks that fail are reported as warnings. If no checks fail nothing is reported. The command returns 1 if there are no warnings for use in scripts.} \
  -arg_help {
    -verbose {Show offending objects rather than just error counts.}
    -unconstrained_endpoints {Check path endpoints for timing constraints (timing check or `set_output_delay`).}
    -multiple_clock {Check register/latch clock pins for multiple clocks.}
    -no_clock {Check register/latch clock pins for a clock.}
    -no_input_delay {Check for inputs that do not have a `set_input_delay` command.}
    -no_output_delay {Check for outputs that do not have a `set_output_delay` command.}
    -no_output_delay {Check for outputs that do not have a `set_output_delay` command.}
    -loops {Check for combinational logic loops.}
    -generated_clocks {Check that generated clock source pins have been defined as clocks.}
  }

The script etc/WriteCmdDocs.tcl extracts the command documentation
from the sources and writes doc/Commands.mv, doc/Variables.md, and doc/CommandLine.md.
