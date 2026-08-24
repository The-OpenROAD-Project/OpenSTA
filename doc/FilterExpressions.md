# Filter expressions

The following commands support filtering returned objects by property
values: `get_cells`, `get_clocks`, `get_lib_cells`, `get_lib_pins`,
`get_libs`, `get_modes`, `get_nets`, `get_pins`, `get_ports`,
`get_scenes`, and `get_timing_edges`.

Supported filter expressions are shown below.

| Expression | Description |
| --- | --- |
| `property` | Return objects with `property` value equal to 1. |
| `property==value` | Return objects with `property` value equal to `value`. |
| `property=~pattern` | Return objects with `property` value that matches `pattern`. |
| `property!=value` | Return objects with `property` value not equal to `value`. |
| `property!~pattern` | Return objects with `property` value that does not match `pattern`. |
| `expr1&&expr2` | Return objects that match `expr1` and `expr2`. |
| <code>expr1&#124;&#124;expr2</code> | Return objects that match `expr1` or `expr2`. |

Where `property` is a property supported by the `get_property` command.
If there are spaces in the expression it must be enclosed in quotes so
that it is a single argument.
