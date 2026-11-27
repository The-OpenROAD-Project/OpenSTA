# get_ports with patterns that do and do not contain wildcards.
source wide_ports_design.tcl
file mkdir results
write_wide_ports_design results/find_ports_literal.v find_ports_literal 2000
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog results/find_ports_literal.v
link_design find_ports_literal

proc show { args } {
  set ports [eval [linsert $args 0 get_ports]]
  set names [lsort [lmap port $ports {get_full_name $port}]]
  puts "get_ports $args: [llength $ports] $names"
}

show in0
show in1999
show out17
show clk
show CLK
show d
show q
show d\[3\]
show d\[5:2\]
show d\[2:5\]
show d\[*\]
show d\[9\]
show {esc\[3\]}
show {esc[3]}
show {a/b}
show {a/b_out}
show {a\/b}
show {a\/b_out}
show -quiet nosuch
show nosuch
show -quiet nosuch\[0\]
show -quiet in3\[0\]
show in1?
show in199*
show {in1[0-9]}
show -regexp {in1[0-9]0}
show -regexp in17
show -regexp -nocase clk
show -nocase clk
show {in3 out3 d[0]}

# A literal pattern returns the same object as a scan does.
proc scan_port { name } {
  foreach port [get_ports -quiet *] {
    if { [get_full_name $port] == $name } {
      return $port
    }
  }
  return ""
}
foreach name {in0 in1234 out1999 clk CLK d[0] d[7] q[3] esc[3] a/b} {
  set lookup [get_ports -quiet [string map {/ \\/} $name]]
  set scan [scan_port $name]
  puts "$name: [expr {$lookup ne "" && $lookup == $scan ? "same" : "differ"}]"
}
