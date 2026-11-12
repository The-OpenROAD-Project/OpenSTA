# Signed port and net declarations.
read_liberty asap7_small.lib.gz
read_verilog verilog_signed.v
link_design top
foreach pin {g/A h/A u/g/A} {
  puts "$pin [get_full_name [get_nets -of_objects [get_pins $pin]]]"
}
