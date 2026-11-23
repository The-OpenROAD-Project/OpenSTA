# with_output_to_variable evaluates its commands in the caller's scope
# and passes errors on to the caller.

proc capture_in_proc { prefix } {
  set suffix "local"
  with_output_to_variable result { puts "$prefix $suffix" }
  return $result
}

puts "--- caller local variables ---"
puts -nonewline [capture_in_proc "proc"]

puts "--- caller sets variables ---"
with_output_to_variable output { set from_body 42; puts "set" }
puts "from_body: $from_body"
puts -nonewline "output: $output"

puts "--- return value ---"
puts "return: [with_output_to_variable output { puts "out"; expr 6 * 7 }]"

puts "--- error ---"
set code [catch {
  with_output_to_variable output { puts "before error"; error "body failed" }
} msg]
puts "code: $code"
puts "msg: $msg"
puts -nonewline "output: $output"

puts "--- output after error is not redirected ---"
puts "visible"
