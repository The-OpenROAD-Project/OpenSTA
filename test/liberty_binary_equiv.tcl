# The library read from a .blib matches the library read from the text
# .lib: both are dumped with write_liberty and compared. Libraries stay
# loaded across clear_sta, so each library is selected by object.
source helpers.tcl

proc read_liberty_new { lib_file } {
  set before [get_libs -quiet *]
  read_liberty $lib_file
  foreach lib [get_libs *] {
    if { $lib ni $before } {
      return $lib
    }
  }
  error "no library read from $lib_file"
}

proc check_binary_equiv { lib_file } {
  set stem [file rootname [file rootname [file tail $lib_file]]]
  set text_dump [make_result_file ${stem}_text.lib]
  set blib_dump [make_result_file ${stem}_blib.lib]
  # Relative so the "library already exists" warning is reproducible.
  set blib_file results/$stem.blib

  set text_lib [read_liberty_new $lib_file]
  sta::write_liberty_cmd $text_lib $text_dump

  write_liberty_binary $lib_file $blib_file
  set blib_lib [read_liberty_new $blib_file]
  sta::write_liberty_cmd $blib_lib $blib_dump

  puts "[get_name $text_lib]: [file tail $lib_file] vs $blib_file"
  diff_files $text_dump $blib_dump
}

check_binary_equiv liberty_latch3.lib
check_binary_equiv liberty_retain.lib
check_binary_equiv non_seq_timing.lib
check_binary_equiv asap7_small.lib.gz
