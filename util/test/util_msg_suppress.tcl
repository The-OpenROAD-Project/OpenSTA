# Test message suppress/unsuppress
# suppress_msg and unsuppress_msg take message IDs as arguments

suppress_msg 999
puts "PASS: suppress_msg accepted"

unsuppress_msg 999
puts "PASS: unsuppress_msg accepted"

# Test with multiple IDs
suppress_msg 100 200
puts "PASS: suppress_msg multiple IDs accepted"

unsuppress_msg 100 200
puts "PASS: unsuppress_msg multiple IDs accepted"

puts "ALL PASSED"
