Purpose:
- Verifies multi-level propagation through two gates.

What it checks:
- Internal wire updates
- Cascaded delays
- OR gate behavior when one input stays high

Expected behavior:
- Y becomes 1 after w1 becomes 1 and the OR delay elapses.
- Y stays high while C is 1 even if w1 later goes low.
- Y goes low only after both w1 and C are 0.