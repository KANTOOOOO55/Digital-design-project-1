module test3_xor_chain (A, B, C, Y);
  input A, B, C;
  output Y;
  wire w1;

  xor  #(4) G0(w1, A, B);
  xnor #(2) G1(Y, w1, C);
endmodule