# write_liberty_binary records a reproducible source name and content hash.
source helpers.tcl

# Report an error message with the results directory stripped.
proc puts_error { msg } {
  global result_dir
  puts [string map [list "$result_dir/" ""] $msg]
}

# FNV-1a 64-bit hash of a file's bytes, as 16 hex digits.
proc file_hash { filename } {
  set stream [open $filename rb]
  set bytes [read $stream]
  close $stream
  binary scan $bytes cu* byte_list
  set hash 14695981039346656037
  foreach byte $byte_list {
    set hash [expr { (($hash ^ $byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF }]
  }
  return [format %08x%08x [expr { $hash >> 32 }] [expr { $hash & 0xFFFFFFFF }]]
}

set blib_file [make_result_file liberty_binary_source.blib]
write_liberty_binary liberty_float_as_str.lib $blib_file
set info [liberty_binary_info $blib_file]
puts "version: [dict get $info version]"
set source_file [dict get $info source_file]
puts "source file: $source_file"
puts "source file is basename: [expr { $source_file eq [file tail $source_file] }]"
puts "source hash matches: [expr { [dict get $info source_hash] eq [file_hash liberty_float_as_str.lib] }]"

# A gzipped source is hashed as stored on disk.
set gz_blib [make_result_file liberty_binary_source_gz.blib]
write_liberty_binary ../examples/asap7_small_ff.lib.gz $gz_blib
set gz_info [liberty_binary_info $gz_blib]
puts "gzipped source file: [dict get $gz_info source_file]"
puts "gzipped source hash matches: [expr { [dict get $gz_info source_hash] eq [file_hash ../examples/asap7_small_ff.lib.gz] }]"

# Absolute inputs in separate checkouts must produce the same bytes as the
# relative input above. Include a directory with spaces and a different length.
set stream [open $blib_file rb]
set reference_bytes [read $stream]
close $stream
set copies_dir [make_result_file liberty_binary_source_copies]
set copies_match 1
foreach checkout {checkout_a {another checkout}} {
  set copy_dir $copies_dir/$checkout
  file mkdir $copy_dir
  set copy_source $copy_dir/liberty_float_as_str.lib
  file copy -force liberty_float_as_str.lib $copy_source
  set copy_blib $copy_dir/output.blib
  write_liberty_binary $copy_source $copy_blib
  set stream [open $copy_blib rb]
  if { [read $stream] ne $reference_bytes } { set copies_match 0 }
  close $stream
}
puts "cross-directory binaries match: $copies_match"

# Files with the same basename are distinguished by their content hash.
set stream [open $copy_source a]
puts $stream "/* different source bytes */"
close $stream
write_liberty_binary $copy_source $copy_blib
set changed_info [liberty_binary_info $copy_blib]
puts "same basename retained: [expr { [dict get $changed_info source_file] eq $source_file }]"
puts "different content changes source hash: [expr { [dict get $changed_info source_hash] ne [dict get $info source_hash] }]"

# Hash the actual input through a symlink followed by "..", while recording
# only its basename rather than any part of the directory path.
set source_dir [make_result_file liberty_binary_source_path]
file mkdir $source_dir/real/child
file copy -force liberty_float_as_str.lib $source_dir/real/source.lib
file delete -force $source_dir/alias
file link -symbolic $source_dir/alias $source_dir/real/child
set alias_blib [make_result_file liberty_binary_source_alias.blib]
write_liberty_binary $source_dir/alias/../source.lib $alias_blib
set alias_info [liberty_binary_info $alias_blib]
set recorded_source [dict get $alias_info source_file]
puts "symlink source matches: [expr { $recorded_source eq "source.lib" }]"
puts "symlink source hash matches: [expr { [dict get $alias_info source_hash] eq [file_hash $source_dir/real/source.lib] }]"

# Only binary liberty files have a header to report.
catch { liberty_binary_info liberty_float_as_str.lib } result
puts_error $result
catch { liberty_binary_info [make_result_file does_not_exist.blib] } result
puts_error $result
