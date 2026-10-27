source helpers.tcl
read_liberty asap7_small.lib.gz

# Reconnect ports after linking so reader alias merging cannot hide the case.
proc load_input_alias {} {
  read_verilog verilog_input_alias.v
  link_design alias_top
  foreach {net port} {internal_input external_input
                     internal_output external_output
                     internal_io external_io} {
    if {![connect_pin $net $port]} {
      error "could not connect $port to $net"
    }
  }
}

proc input_alias_file_bytes {filename} {
  set stream [open $filename rb]
  set bytes [read $stream]
  close $stream
  return $bytes
}
