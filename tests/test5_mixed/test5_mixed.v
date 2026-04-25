module test5_mixed (A, B, C, D, Y);
  input A, B, C, D;
  output Y;
  wire w1, w2, w3;

  nand #(2) G0(w1, A, B);
  nor  #(2) G1(w2, C, D);
  xor  #(3) G2(w3, w1, w2);
  buf  #(1) G3(Y, w3);
endmodule