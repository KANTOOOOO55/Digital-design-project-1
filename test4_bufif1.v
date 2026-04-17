module test4_bufif1 (D, EN, Y);
  input D, EN;
  output Y;

  bufif1 #(3) G0(Y, D, EN);
endmodule