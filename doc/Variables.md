# Variables

This page is generated from `define_var_help`.
Do not edit it by hand; rebuild `sta` to regenerate.

Use `help <variable>` in the Tcl interpreter for the same text.

## hierarchy_separator

```
hierarchy_separator / @ ^ # . |
```

The `hierarchy_separator` separates instance names in a hierarchical instance, net, or pin name. The default value is '/'.

## sta_cond_default_arcs_enabled

```
sta_cond_default_arcs_enabled 0|1
```

When set to 0, default timing arcs with no condition (Liberty timing arcs with no when expression) are disabled if there are other conditional timing arcs between the same pins. The default value is 1.

## sta_continue_on_error

```
sta_continue_on_error 0|1
```

The `include` and `read_sdc` commands stop and report any errors encountered while reading a file unless `sta_continue_on_error` is 1. The default value is 0.

## sta_crpr_enabled

```
sta_crpr_enabled 0|1
```

During min/max timing analysis for on_chip_variation the data and clock paths may overlap. For a setup check the maximum path delays are used for the data and the minimum path delays are used for the clock. Because the gates cannot simultaneously have minimum and maximum delays the timing check slack is pessimistic. This pessimism is known as Common Reconvergent Pessimism Removal, or CRPR. Enabling CRPR slows down the analysis. The default value is 1.

## sta_crpr_mode

```
sta_crpr_mode same_pin|same_transition
```

When the data and clock paths of a timing check overlap (see `sta_crpr_enabled`), pessimism is removed independent of the path rise/fall transitions. When `sta_crpr_mode` is `same_transition`, the pessimism is only removed if the path rise/fall transitions are the same. The default value is `same_pin`.

## sta_dynamic_loop_breaking

```
sta_dynamic_loop_breaking 0|1
```

When `sta_dynamic_loop_breaking` is 0, combinational logic loops are disabled by disabling a timing arc that closes the loop. When `sta_dynamic_loop_breaking` is 1, all paths around the loop are reported. The default value is 0.

## sta_gated_clock_checks_enabled

```
sta_gated_clock_checks_enabled 0|1
```

When `sta_gated_clock_checks_enabled` is 1, clock gating setup and hold timing checks are checked. The default value is 1.

## sta_input_port_default_clock

```
sta_input_port_default_clock 0|1
```

When `sta_input_port_default_clock` is 1 a default input arrival is added for input ports that do not have an arrival time specified with the `set_input_delay` command. The default value is 0.

## sta_internal_bidirect_instance_paths_enabled

```
sta_internal_bidirect_instance_paths_enabled 0|1
```

When set to 0, paths from bidirectional (inout) ports back into the instance are disabled. When set to 1, paths from bidirectional ports back into the instance are enabled. The default value is 0.

## sta_pocv_mode

```
sta_pocv_mode scalar|normal|skew_normal
```

Enable parametric on chip variation using statistical timing analysis. The default value is `scalar`.

## sta_pocv_quantile

```
sta_pocv_quantile float
```

The target quantile of a delay probability distribution (confidence level). The default value is 3 standard deviations, or sigma.

## sta_preset_clear_arcs_enabled

```
sta_preset_clear_arcs_enabled 0|1
```

When set to 1, paths through asynchronous preset and clear timing arcs are searched. The default value is 0.

## sta_propagate_all_clocks

```
sta_propagate_all_clocks 0|1
```

All clocks defined after `sta_propagate_all_clocks` is set to 1 are propagated. If it is set before any clocks are defined it has the same effect as

```
set_propagated_clock [all_clocks]
```

after all clocks have been defined. The default value is 0.

## sta_propagate_gated_clock_enable

```
sta_propagate_gated_clock_enable 0|1
```

When set to 1, paths of gated clock enables are propagated through the clock gating instances. If the gated clock controls sequential elements setting `sta_propagate_gated_clock_enable` to 0 prevents spurious paths from the clock enable. The default value is 1.

## sta_recovery_removal_checks_enabled

```
sta_recovery_removal_checks_enabled 0|1
```

When `sta_recovery_removal_checks_enabled` is 0, recovery and removal timing checks are disabled. The default value is 1.

## sta_report_default_digits

```
sta_report_default_digits integer
```

The number of digits to print after a decimal point. The default value is 2.

