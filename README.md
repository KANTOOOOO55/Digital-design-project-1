# Event-Driven Logic Circuit Simulator

This project implements an **event-driven simulator for structural Verilog circuits**.  
The simulator reads a Verilog circuit description and a stimuli file, then produces a simulation output file showing how signals change over time according to gate delays.

---

## Features

- supports structural Verilog primitive gates
- processes input changes using an **event-driven approach**
- handles gate propagation delays
- generates a `.sim` output file with timed signal transitions
- includes sample input files for testing
- includes additional test circuits in the `tests/` folder

---

## Supported Primitives

The simulator currently supports the following logic primitives:

- `and`
- `or`
- `xor`
- `nand`
- `nor`
- `xnor`
- `buf`
- `bufif1`
- `not`

---

## Input Files

The simulator uses two input files:

1. **Verilog circuit file** (`.v`)  
   Contains the structural Verilog module, inputs, outputs, wires, and gate instances.

2. **Stimuli file** (`.stim`)  
   Contains timed input changes in the form:

   ```txt
   #0 A=0;
   #100 A=1;
   #50 B=1;
   ```

The delay values in the stimuli file are interpreted **relative to the previous event**.

---

## Output File

The simulator produces a simulation output file (`.sim`) that records signal changes in the format:

```txt
time, signal, value
```

Example:

```txt
0, A, 0
2, w1, 1
5, Y, 1
```

---

## Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

---

#### Run Instructions

```bash
./EventDrivenSimulator circuit.v stimuli.stim output.sim
```

Or run with no arguments to use the bundled example files:

```bash
./EventDrivenSimulator
```
You can also run the program with **no command-line arguments** to use the sample files:

- `example.v`
- `example.stim`

---

## Project Structure

```text
.
├── CMakeLists.txt
├── README.md
├── main.cpp
├── example.v
├── example.stim
├── example.sim
├── simulator_gui.html
├── cmake/
│   └── run_test.cmake
├── .github/
│   └── workflows/
│       └── ci.yml
├── tests/
│   ├── test1_inverter/
│   ├── test2_and_or/
│   ├── test3_xor_chain/
│   ├── test4_bufif1/
│   └── test5_mixed/
└── report/
    └── report.md
```

## How the Simulator Works

The simulator follows these main steps:

1. parses the Verilog file
2. stores inputs, outputs, wires, and gates
3. parses the stimuli file
4. converts relative delays into absolute simulation times
5. pushes input changes into a priority queue
6. processes events in time order
7. reevaluates only the gates affected by each signal change
8. writes all real signal transitions to the output file

This design is more efficient than recomputing the entire circuit at every time step.

---

## Example

### Example Verilog file
```verilog
module test2 (A, B, C, D, Y);
  input A, B, C, D;
  output Y;
  wire w1, w2, w3;

  and  #(2) G0(w1, A, B);
  not  #(1) G1(w2, C);
  xor  #(3) G2(w3, w1, w2);
  or   #(2) G3(Y, w3, D);
endmodule
```

### Example stimuli file
```txt
#0 A=0;
#0 B=0;
#0 C=0;
#0 D=0;
#100 A=1;
#100 B=1;
#150 C=1;
#100 D=1;
#200 A=0;
#100 C=0;
#100 D=0;
#100 B=0;
```

### Example output
```txt
0, A, 0
0, B, 0
0, C, 0
0, D, 0
1, w2, 1
2, w1, 0
5, w3, 1
7, Y, 1
```

---


---

## Bonus: Waveform GUI

Open `simulator_gui.html` in any browser — no install needed. Paste or upload `.v` and `.stim` files, click **Run Simulation**, and inspect interactive color-coded waveforms. Import a `.sim` file directly or export simulation output.

---

---

## How It Works

1. Parse the Verilog file → gate list, input/output/wire sets
2. Parse the stimuli file → convert relative delays to absolute timestamps
3. Push all input events onto a min-heap priority queue
4. Pop the earliest event; if it changes the signal, record it and propagate
5. Use a fanout map to find only the gates affected by the changed signal
6. Re-evaluate affected gates and schedule output events with gate delay
7. Repeat until the queue is empty

---

## Limitations

The current implementation is suitable for the project requirements, but some limitations remain:

- it supports only the primitive gate set required for the project
- it does not implement full Verilog syntax
- it does not implement full advanced four-valued Verilog semantics
- waveform visualization is not included unless added separately as an extra feature

---

## Authors

- Member 1: _Habiba Khashaba_
- Member 2: _Omar Hassan_

---

## Notes

This project was developed for a digital design course project on **event-driven logic simulation**.  
