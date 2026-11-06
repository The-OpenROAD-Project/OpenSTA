# A search for clock arrivals only (report_clock_skew) stops at the
# register clock pins with more than one thread: the clock pins of
# r1, r2 and r3 share a level, which is visited in parallel, and the
# search must not carry on from them into the data network.
sta::set_thread_count 2
read_liberty ../../examples/nangate45_slow.lib.gz
read_verilog ../../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}

proc path_count { pin_name } {
  set count 0
  foreach vertex [[get_pins $pin_name] vertices] {
    if { $vertex != "NULL" } {
      foreach rf {rise fall} {
        foreach min_max {min max} {
          set iter [$vertex path_iterator $rf $min_max]
          while { [$iter has_next] } {
            $iter next
            incr count
          }
          $iter finish
        }
      }
    }
  }
  return $count
}

report_clock_skew
foreach pin {r1/CK r1/Q r2/Q u1/Z u2/ZN r3/D} {
  set count [path_count $pin]
  puts "$pin: $count paths"
  if { $pin == "r1/CK" && $count == 0 } {
    error "no clock arrival at $pin"
  }
  if { $pin != "r1/CK" && $count != 0 } {
    error "the search for clock arrivals reached the data pin $pin"
  }
}
