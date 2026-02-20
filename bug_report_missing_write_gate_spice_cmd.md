# Bug Report: Missing SWIG binding for `write_gate_spice_cmd`

## Summary

`write_gate_spice` Tcl command always fails with `invalid command name "write_gate_spice_cmd"` because the underlying C++ SWIG binding is not registered.

## Details

- **`write_gate_spice`** is defined as a Tcl proc in `spice/WriteSpice.tcl:138`
- It calls `write_gate_spice_cmd` at line 198 to invoke the C++ implementation
- **`write_gate_spice_cmd` is not registered** in any Tcl namespace (neither global nor `::sta::`)
- In contrast, `write_path_spice_cmd` IS correctly registered as `::sta::write_path_spice_cmd`

## Reproduction

```tcl
sta::sta -no_init -no_splash -exit /dev/stdin <<'EOF'
puts [info commands ::sta::write_path_spice_cmd]  ;# returns ::sta::write_path_spice_cmd
puts [info commands ::sta::write_gate_spice_cmd]  ;# returns empty
EOF
```

## Impact

All `write_gate_spice` calls silently fail when wrapped in `catch`, or crash `sta` when called directly. The C++ implementation likely exists in `WriteSpice.cc` but the SWIG `.i` file is missing the binding declaration for `write_gate_spice_cmd`.

## Affected Test Files

The following test files had `write_gate_spice` catch blocks that always produced error output:
- `spice/test/spice_gate_advanced.tcl`
- `spice/test/spice_gate_cells.tcl`
- `spice/test/spice_gcd_gate.tcl`
- `spice/test/spice_gcd_path.tcl`
- `spice/test/spice_multipath.tcl`
- `spice/test/spice_subckt_file.tcl`
- `spice/test/spice_write_options.tcl`
