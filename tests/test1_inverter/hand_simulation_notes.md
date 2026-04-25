Purpose:
- Verifies a single inverter with one gate delay.

What it checks:
- Basic event scheduling
- Correct propagation through a single gate
- Correct output timestamps

Expected behavior:
- When A becomes 0 at time 0, Y becomes 1 after 5 ps.
- When A becomes 1 at time 100, Y becomes 0 at time 105.
- When A becomes 0 at time 200, Y becomes 1 at time 205.