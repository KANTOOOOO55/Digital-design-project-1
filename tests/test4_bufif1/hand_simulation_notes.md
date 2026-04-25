Purpose:
- Verifies tri-state buffer behavior.

What it checks:
- bufif1 handling
- z output when enable is 0
- propagation of data when enable is 1

Expected behavior:
- Y is z while EN = 0.
- Y follows D only when EN = 1.