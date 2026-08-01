# Malformed port attributes must produce a clean warning, not crash the reader.
# Regression for LibertyReader null-dereferences:
#   - makePortFuncs(): "function" expression that parses to null (undeclared pin)
#   - makePortFuncs(): "three_state" expression that parses to null
#   - makeBundlePort(): "bundle" group with no "members" attribute
# Before the fix each of these dereferenced a null pointer and crashed (SIGSEGV).
read_liberty liberty_malformed_ports.lib
puts "read_liberty completed, cells loaded: [llength [get_lib_cells malformed_ports/*]]"
