module test2 (A, B, C, D, Y);
  input A, B, C, D;
  output Y;
  wire w1, w2, w3;

  and  #(2) G0(w1, A, B);
  not  #(1) G1(w2, C);
  xor  #(3) G2(w3, w1, w2);
  or   #(2) G3(Y, w3, D);
endmodule