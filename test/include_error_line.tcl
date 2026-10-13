# An error in a multi-line command is reported at its first line.
catch { read_sdc include_error_line.sdc } msg
puts $msg

# An error without a traceback does not reuse the read_sdc traceback.
catch { include include_error_line_missing.tcl } msg
puts [string first include_error_line_undefined_cmd $::errorInfo]
