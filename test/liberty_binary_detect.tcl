# Binary liberty files are recognized by their contents.
source helpers.tcl

set renamed_file [make_result_file liberty_binary_detect.lib]
write_liberty_binary liberty_float_as_str.lib $renamed_file
read_liberty $renamed_file
report_units
puts "cells: [llength [get_lib_cells *]]"

# A text liberty file named .blib is read as text.
set text_blib [make_result_file liberty_binary_detect_text.blib]
file copy -force liberty_backslash_eol.lib $text_blib
read_liberty $text_blib
puts "text cells: [llength [get_lib_cells liberty_backslash_eol/*]]"

# Probe decompressed contents and retain the probe when reading a pipe.
set stream [open liberty_float_as_str.lib rb]
set source_text [read $stream]
close $stream
foreach {format transport} {
  text_gzip file
  binary_gzip file
  text fifo
  text_gzip fifo
  binary fifo
  binary_gzip fifo
} {
  set lib_name liberty_binary_detect_${format}_${transport}
  set source_file [make_result_file ${lib_name}.lib]
  set stream [open $source_file wb]
  puts -nonewline $stream [string map [list liberty_float_as_str $lib_name] $source_text]
  close $stream
  set input_file $source_file
  if { [string match binary* $format] } {
    set input_file [make_result_file ${lib_name}.blib]
    write_liberty_binary $source_file $input_file
  }
  if { [string match *_gzip $format] } {
    set stream [open $input_file rb]
    set compressed [zlib gzip [read $stream]]
    close $stream
    # No .gz extension: compression and format detection both use contents.
    set compressed_file [make_result_file ${lib_name}.data]
    set stream [open $compressed_file wb]
    puts -nonewline $stream $compressed
    close $stream
    if { [string match binary* $format] } {
      if { [liberty_binary_info $compressed_file] ne [liberty_binary_info $input_file] } {
        error "compressed binary header differs"
      }
    }
    set input_file $compressed_file
  }
  if { $transport eq "fifo" } {
    set fifo [make_result_file ${lib_name}.pipe]
    file delete -force $fifo
    exec mkfifo $fifo
    # Open the FIFO in the child shell, so starting the producer does not block.
    exec sh -c {exec cat "$1" > "$2"} sh $input_file $fifo &
    try {
      read_liberty $fifo
    } finally {
      file delete -force $fifo
    }
  } else {
    read_liberty $input_file
  }
  puts "$format $transport cells: [llength [get_lib_cells ${lib_name}/*]]"
}
