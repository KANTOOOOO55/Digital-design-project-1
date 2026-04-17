module test1_inverter (A, Y);
  input A;
  output Y;

  not #(5) G0(Y, A);
endmodule