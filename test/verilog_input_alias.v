module alias_top(external_input, other_input, external_output, external_io);
  input external_input, other_input;
  output external_output;
  inout external_io;
  wire internal_input, internal_output, internal_io;
  supply1 power_net;
  supply0 ground_net;
  BUFx2_ASAP7_75t_R buffer (.A(internal_input), .Y(internal_output));
  BUFx2_ASAP7_75t_R io_buffer (.A(internal_io), .Y());
endmodule
