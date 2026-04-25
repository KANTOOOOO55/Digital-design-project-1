module test2_and_or (A, B, C, Y);
  input A, B, C;
  output Y;
  wire w1;

  and #(2) G0(w1, A, B);
  or  #(3) G1(Y, w1, C);
endmodule