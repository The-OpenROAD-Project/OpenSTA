# Test message suppress/unsuppress
# suppress_msg and unsuppress_msg take message IDs as arguments

suppress_msg 999
unsuppress_msg 999

# Test with multiple IDs
suppress_msg 100 200
unsuppress_msg 100 200

# Verify command flow remains functional after suppression changes.
with_output_to_variable units_before { report_units }
if {[string length $units_before] == 0} {
  error "report_units unexpectedly empty before suppress/unsuppress checks"
}

suppress_msg 10 11 12
unsuppress_msg 10 11 12

with_output_to_variable units_after { report_units }
if {[string length $units_after] == 0} {
  error "report_units unexpectedly empty after suppress/unsuppress checks"
}
