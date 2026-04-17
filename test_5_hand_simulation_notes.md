Purpose:
- Verifies a mixed circuit with four gate types and several internal transitions.

What it checks:
- nand, nor, xor, and buf primitives
- multi-level timing
- internal wire interactions

Expected behavior:
- Initial values create both w1 and w2 = 1.
- The XOR and final buffer produce Y after the full delay chain.
- Later transitions verify the circuit continues to schedule events correctly.