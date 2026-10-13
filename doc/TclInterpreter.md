# Tcl interpreter

Keyword arguments to commands may be abbreviated. For example,

```tcl
report_checks -unique
```

is equivalent to

```tcl
report_checks -unique_paths_to_endpoint
```

The `help` command lists matching commands and their arguments.

```tcl
% help report_checks
report_checks [-from from_list|-rise_from from_list|-fall_from from_list]
  ...
```

Use `help -verbose <pattern>` to print the full description and option
list. Use `help <variable>` for documented `sta_*` variables.

Many reporting commands support redirection of the output to a file
much like a Unix shell.

```tcl
report_checks -to out1 > path.log
report_checks -to out2 >> path.log
```
