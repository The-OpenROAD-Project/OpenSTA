# Error handling for binary liberty files.
source helpers.tcl

# Report an error message with the results directory stripped.
proc puts_error { msg } {
  global result_dir
  puts [string map [list "$result_dir/" ""] $msg]
}

# The index the next string added to a .blib string table gets, which is the
# current entry count.
proc blib_next_string_index { blib_file } {
  set stream [open $blib_file rb]
  set bytes [read $stream]
  close $stream
  binary scan [string range $bytes 12 15] iu path_length
  binary scan [string range $bytes [expr { 24 + $path_length }] end] wu table_offset
  binary scan [string range $bytes $table_offset end] iu string_count
  return $string_count
}

# Insert records before the library's end tag in a .blib and add name to its
# string table at blib_next_string_index.
proc insert_blib_records { blib_file records name } {
  set stream [open $blib_file rb]
  set bytes [read $stream]
  close $stream
  binary scan [string range $bytes 12 15] iu path_length
  set offset_location [expr { 24 + $path_length }]
  binary scan [string range $bytes $offset_location [expr { $offset_location + 7 }]] wu table_offset
  binary scan [string range $bytes $table_offset [expr { $table_offset + 3 }]] iu string_count
  # The library's end tag and the EOF tag come just before the string table.
  set insert_offset [expr { $table_offset - 2 }]
  set bytes "[string range $bytes 0 [expr { $insert_offset - 1 }]]$records[string range $bytes $insert_offset end]"
  incr table_offset [string length $records]
  set bytes [string replace $bytes $offset_location [expr { $offset_location + 7 }] [binary format wu $table_offset]]
  set bytes [string replace $bytes $table_offset [expr { $table_offset + 3 }] [binary format iu [expr { $string_count + 1 }]]]
  append bytes [binary format iu [string length $name]] $name [binary format iu $string_count]
  set stream [open $blib_file wb]
  puts -nonewline $stream $bytes
  close $stream
}

# A truncated .blib is rejected rather than partially loaded.
set blib_file [make_result_file liberty_binary_errors.blib]
write_liberty_binary liberty_float_as_str.lib $blib_file
set stream [open $blib_file r+]
chan truncate $stream [expr { [file size $blib_file] / 2 }]
close $stream
catch { read_liberty $blib_file } result
puts_error $result
puts "libraries loaded: [llength [get_libs -quiet *]]"

# A .blib written by another format version is rejected with its version.
set old_blib [make_result_file liberty_binary_errors_v1.blib]
write_liberty_binary liberty_float_as_str.lib $old_blib
set stream [open $old_blib r+]
fconfigure $stream -translation binary
seek $stream 8
puts -nonewline $stream [binary format iu 1]
close $stream
catch { read_liberty $old_blib } result
puts_error $result

# A corrupt record inside the library (its end tag) is rejected.
set body_blib [make_result_file liberty_binary_errors_body.blib]
write_liberty_binary liberty_float_as_str.lib $body_blib
set stream [open $body_blib r+]
fconfigure $stream -translation binary
# The header ends with the string table offset, after the source path. The
# library's end tag and the EOF tag come just before the string table.
seek $stream 12
binary scan [read $stream 4] iu path_length
seek $stream [expr { 24 + $path_length }]
binary scan [read $stream 8] wu string_table_offset
seek $stream [expr { $string_table_offset - 2 }]
puts -nonewline $stream [binary format c 255]
close $stream
catch { read_liberty $body_blib } result
puts_error $result

# A string-table index used twice is rejected. Otherwise one string would
# silently replace another throughout the library.
set dup_blib [make_result_file liberty_binary_errors_dup_index.blib]
write_liberty_binary liberty_float_as_str.lib $dup_blib
set stream [open $dup_blib rb]
set bytes [read $stream]
close $stream
binary scan [string range $bytes 12 15] iu path_length
binary scan [string range $bytes [expr { 24 + $path_length }] end] wu table_offset
# The table is an entry count, then per entry the string length, its bytes
# and its index. Give the second entry the first entry's index.
set first_entry [expr { $table_offset + 4 }]
binary scan [string range $bytes $first_entry end] iu length
set first_index [expr { $first_entry + 4 + $length }]
set second_entry [expr { $first_index + 4 }]
binary scan [string range $bytes $second_entry end] iu length
set second_index [expr { $second_entry + 4 + $length }]
set bytes [string replace $bytes $second_index [expr { $second_index + 3 }] \
             [string range $bytes $first_index [expr { $first_index + 3 }]]]
set stream [open $dup_blib wb]
puts -nonewline $stream $bytes
close $stream
catch { read_liberty $dup_blib } result
puts_error $result

# The string table ends the file, so bytes after it are rejected.
set after_table_blib [make_result_file liberty_binary_errors_after_table.blib]
write_liberty_binary liberty_float_as_str.lib $after_table_blib
set stream [open $after_table_blib ab]
puts -nonewline $stream [binary format c 0]
close $stream
catch { read_liberty $after_table_blib } result
puts_error $result

# The body must contain one complete group and EOF exactly at its boundary.
set stream [open liberty_float_as_str.lib rb]
set source_text [read $stream]
close $stream
foreach corruption {early_eof trailing missing_eof empty_body} {
  set stem liberty_binary_errors_$corruption
  set source_file [make_result_file ${stem}.lib]
  set stream [open $source_file wb]
  puts -nonewline $stream [string map [list liberty_float_as_str $stem] $source_text]
  close $stream
  set bad_blib [make_result_file ${stem}.blib]
  write_liberty_binary $source_file $bad_blib
  set stream [open $bad_blib rb]
  set bytes [read $stream]
  close $stream
  binary scan [string range $bytes 12 15] iu path_length
  set offset_location [expr { 24 + $path_length }]
  binary scan [string range $bytes $offset_location [expr { $offset_location + 7 }]] wu table_offset
  set body_start [expr { 32 + $path_length }]
  switch $corruption {
    early_eof {
      set bytes [string replace $bytes $body_start $body_start [binary format c 0]]
    }
    trailing {
      set bytes "[string range $bytes 0 [expr { $table_offset - 1 }]][binary format c 255][string range $bytes $table_offset end]"
      incr table_offset
    }
    missing_eof {
      # Pad the string table to 256 entries so its first byte is zero. An
      # unbounded reader would mistake that byte for the missing EOF tag.
      binary scan [string range $bytes $table_offset [expr { $table_offset + 3 }]] iu count
      if { $count > 256 } { error "fixture string table is too large" }
      for {set i $count} {$i < 256} {incr i} {
        set entry padding_$i
        append bytes [binary format iu [string length $entry]] $entry [binary format iu $i]
      }
      set bytes [string replace $bytes $table_offset [expr { $table_offset + 3 }] [binary format iu 256]]
      set eof_offset [expr { $table_offset - 1 }]
      set bytes [string replace $bytes $eof_offset $eof_offset]
      incr table_offset -1
    }
    empty_body {
      set bytes [string replace $bytes $body_start [expr { $table_offset - 2 }]]
      set table_offset [expr { $body_start + 1 }]
    }
  }
  set bytes [string replace $bytes $offset_location [expr { $offset_location + 7 }] [binary format wu $table_offset]]
  set stream [open $bad_blib wb]
  puts -nonewline $stream $bytes
  close $stream
  if { ![catch { read_liberty $bad_blib } result] } {
    error "$corruption was accepted"
  }
  puts_error $result
}

# Bound recursive decoding even when every nested record is well-formed.
# Test one level beyond the limit, a former stack-overflow case, and two
# sibling chains at the limit to check that depth decreases on group exit.
foreach depth {1001 100000 1000} {
  set stem liberty_binary_errors_depth_$depth
  set source_file [make_result_file ${stem}.lib]
  set stream [open $source_file wb]
  puts -nonewline $stream [string map [list liberty_float_as_str $stem] $source_text]
  close $stream
  set depth_blib [make_result_file ${stem}.blib]
  write_liberty_binary $source_file $depth_blib

  # Unknown groups are ignored by LibertyReader. Insert nested groups of a new
  # type before the library's end tag. The library itself counts as the first
  # nesting level.
  set group_type_index [blib_next_string_index $depth_blib]
  set group_begin [binary format ccc 1 0 1]
  append group_begin [binary format iu $group_type_index] [binary format iu 0]
  set nested [string repeat $group_begin [expr { $depth - 1 }]]
  append nested [string repeat [binary format c 2] [expr { $depth - 1 }]]
  if { $depth == 1000 } { append nested $nested }
  insert_blib_records $depth_blib $nested ignored_depth_group

  if { $depth > 1000 } {
    if { ![catch { read_liberty $depth_blib } result] } {
      error "nesting depth $depth was accepted"
    }
    puts_error $result
  } else {
    read_liberty $depth_blib
    puts "nesting limit cells: [llength [get_lib_cells ${stem}/*]]"
  }
}

# Records the writer never emits are rejected: a simple attribute without
# exactly one value, and an empty float sequence, which would read back as a
# float. LibertyReader would ignore the attribute if it were accepted.
foreach corruption {simple_attr_count empty_float_seq} {
  set stem liberty_binary_errors_$corruption
  set source_file [make_result_file ${stem}.lib]
  set stream [open $source_file wb]
  puts -nonewline $stream [string map [list liberty_float_as_str $stem] $source_text]
  close $stream
  set bad_blib [make_result_file ${stem}.blib]
  write_liberty_binary $source_file $bad_blib
  # Records are a tag, a line delta, the name's string index and a value
  # count, followed by the values.
  set name_index [blib_next_string_index $bad_blib]
  if { $corruption == "simple_attr_count" } {
    set record [binary format cccii 3 0 1 $name_index 2][binary format cr 2 1.0]
  } else {
    set record [binary format cccii 4 0 1 $name_index 1][binary format ci 5 0]
  }
  insert_blib_records $bad_blib $record ignored_attr
  if { ![catch { read_liberty $bad_blib } result] } {
    error "$corruption was accepted"
  }
  puts_error $result
}

# A missing .blib reports the file, not a corrupt file.
catch { read_liberty [make_result_file does_not_exist.blib] } result
puts_error $result

# A missing input leaves no partial output behind.
set out_file [make_result_file liberty_binary_errors_missing_input.blib]
catch { write_liberty_binary does_not_exist.lib $out_file } result
puts_error $result
puts "partial output left behind: [file exists $out_file]"

# A syntax error in the input leaves no partial output behind either.
set bad_lib [make_result_file liberty_binary_errors_truncated.lib]
file copy -force liberty_float_as_str.lib $bad_lib
set stream [open $bad_lib r+]
chan truncate $stream [expr { [file size $bad_lib] / 2 }]
close $stream
set out_file [make_result_file liberty_binary_errors_truncated.blib]
catch { write_liberty_binary $bad_lib $out_file } result
puts_error $result
puts "partial output left behind: [file exists $out_file]"

# Writing over the input, by any spelling of its path, is refused.
set same_lib [make_result_file liberty_binary_errors_same.lib]
set same_lib_alias [make_result_file ./liberty_binary_errors_same.lib]
file copy -force liberty_float_as_str.lib $same_lib
catch { write_liberty_binary $same_lib $same_lib } result
puts_error $result
catch { write_liberty_binary $same_lib $same_lib_alias } result
puts_error $result
diff_files liberty_float_as_str.lib $same_lib

# A binary liberty input, compressed or not, is refused.
set blib_input [make_result_file liberty_binary_errors_input.blib]
write_liberty_binary liberty_float_as_str.lib $blib_input
set stream [open $blib_input rb]
set bytes [read $stream]
close $stream
set gz_input [make_result_file liberty_binary_errors_input.blib.gz]
set stream [open $gz_input wb]
puts -nonewline $stream [zlib gzip $bytes]
close $stream
set out_file [make_result_file liberty_binary_errors_input_out.blib]
foreach input [list $blib_input $gz_input] {
  catch { write_liberty_binary $input $out_file } result
  puts_error $result
  puts "output written: [file exists $out_file]"
}

# An unwritable output path is reported.
catch { write_liberty_binary liberty_float_as_str.lib /nonexistent_dir/out.blib } result
puts_error $result
