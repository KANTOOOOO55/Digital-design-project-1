Purpose:
- Verifies XOR and XNOR behavior together.

What it checks:
- Parity-based gate evaluation
- Correct timing across two logic levels
- Output transitions caused by either the wire or direct input changes

Expected behavior:
- w1 = A xor B after 4 ps.
- Y = xnor(w1, C) after another 2 ps.