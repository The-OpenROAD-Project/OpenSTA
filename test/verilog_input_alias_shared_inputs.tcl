source verilog_input_alias_helpers.tcl

# Check both a distinct internal name and a net named after one input.
foreach net {internal_input external_input} {
  load_input_alias
  connect_pin $net external_input
  foreach suffix {"" .gz} {
    set output [make_result_file verilog_input_alias_shared_inputs.v$suffix]
    write_verilog $output
    set previous [input_alias_file_bytes $output]
    if {$previous eq ""} {
      error "valid export was empty"
    }
    connect_pin $net other_input
    set expected "Error: 2749 cannot write Verilog: input ports external_input and other_input share signal net $net"
    if {![catch {write_verilog $output} message] || $message ne $expected} {
      error "expected shared-input rejection, got '$message'"
    }
    if {[input_alias_file_bytes $output] ne $previous} {
      error "rejected export changed an existing output file"
    }
    file delete $output
    if {![catch {write_verilog $output} message] || $message ne $expected} {
      error "expected shared-input rejection for absent output, got '$message'"
    }
    if {[file exists $output]} {
      error "rejected export created an absent output file"
    }
    disconnect_pin $net other_input
  }
}
puts "shared signal inputs: rejected with both ports and net"
puts "rejected writes: plain/gzip outputs preserved and absent paths untouched"
