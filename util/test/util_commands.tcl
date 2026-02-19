# Test utility commands
puts "--- set_cmd_units -time ns -capacitance pF -resistance kOhm ---"
set_cmd_units -time ns -capacitance pF -resistance kOhm

puts "--- report_units ---"
report_units

puts "--- set_cmd_units -time ps -capacitance fF ---"
set_cmd_units -time ps -capacitance fF

puts "--- report_units after change ---"
report_units

puts "--- set_cmd_units -voltage mV ---"
set_cmd_units -voltage mV

puts "--- set_cmd_units -current uA ---"
set_cmd_units -current uA

puts "--- set_cmd_units -distance um ---"
set_cmd_units -distance um

puts "--- report_units after all changes ---"
report_units

puts "--- elapsed_run_time ---"
set elapsed [elapsed_run_time]
if { $elapsed >= 0 } {
} else {
  puts "FAIL: elapsed_run_time returned negative"
}

puts "--- user_run_time ---"
set user_time [user_run_time]
if { $user_time >= 0 } {
} else {
  puts "FAIL: user_run_time returned negative"
}

puts "--- log_begin / log_end ---"
set log_file "/tmp/sta_test_log_[pid].txt"
log_begin $log_file
puts "test log entry from util_commands"
log_end

# Verify log file was created and has content
if { [file exists $log_file] } {
  set fh [open $log_file r]
  set log_content [read $fh]
  close $fh
  file delete $log_file
  if { [string length $log_content] > 0 } {
  } else {
    puts "INFO: log file was empty"
  }
} else {
  puts "INFO: log file not found (may be expected)"
}

puts "--- suppress_msg / unsuppress_msg ---"
suppress_msg 100 200 300

unsuppress_msg 100 200 300

puts "--- suppress_msg single ---"
suppress_msg 999

unsuppress_msg 999

puts "--- with_output_to_variable ---"
with_output_to_variable result { report_units }
puts "captured output length: [string length $result]"
if { [string length $result] > 0 } {
} else {
  puts "FAIL: with_output_to_variable captured empty output"
}

puts "--- with_output_to_variable second call ---"
with_output_to_variable result2 { puts "hello from with_output_to_variable" }
