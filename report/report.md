# Event-Driven Logic Circuit Simulator Report

## Cover Information
**Course:** CSCE2301 - Digital Design I  
**Project:** Project 1 - Event-Driven Logic Circuits Simulator  
**Title:** Event-Driven Logic Circuit Simulator  
**Team:** _Habiba Khashaba & Omar Hassan_  
**Date:** _Fri 17 Apr_

---

## 1. Introduction
This project implements an event-driven logic circuit simulator for structural Verilog circuits. The simulator reads a Verilog circuit file and a stimuli file, then produces a simulation output file containing the scheduled signal changes over time. The project focuses on discrete event processing, where only changes in signals are propagated through the circuit instead of recomputing the entire design continuously.

The simulator supports the primitive gates required by the project: `and`, `or`, `xor`, `nand`, `nor`, `xnor`, `buf`, `bufif1`, and `not`. Each gate may include a delay value, and the simulator uses those delays when scheduling future events.

---

## 2. Program Design
The simulator is built around a small set of core data structures:

### 2.1 Gate Representation
Each gate is stored with:
- its primitive type
- its instance name
- its propagation delay
- a vector of pin names, where the first pin is the output and the rest are the inputs

This allows the simulator to parse structural Verilog lines such as:

`and #(2) G0(w1, A, B);`

### 2.2 Event Representation
Each event stores:
- the simulation timestamp
- the signal name
- the new signal value
- a sequence number used to break ties when two events have the same timestamp

### 2.3 Signal Table
The simulator stores the current value of every input, output, and wire in a hash table. This makes signal lookup efficient during gate evaluation.

### 2.4 Fanout Map
A fanout map is used to find all gates that depend on a changed signal. When an event changes a signal, the simulator only reevaluates the gates that read that signal. This is the main reason the design is event-driven.

### 2.5 Priority Queue
A priority queue is used for future events. The earliest event is processed first. If two events happen at the same time, the sequence number preserves deterministic ordering.

---

## 3. Algorithms
The main simulation algorithm works as follows:

1. Parse the Verilog file and store declarations and gates.
2. Parse the stimuli file and convert relative delays into absolute timestamps.
3. Push the external stimuli into the priority queue.
4. Repeatedly pop the earliest event from the queue.
5. If the new value is different from the current signal value, update the signal and write the event to the output file.
6. Use the fanout map to find affected gates.
7. Reevaluate each affected gate and schedule output events using that gate's delay.

This approach is more efficient than repeatedly evaluating the whole circuit because it only reacts to actual changes.

---

## 4. Supported Logic Behavior
The simulator supports the project primitives:
- `and`
- `or`
- `xor`
- `nand`
- `nor`
- `xnor`
- `buf`
- `bufif1`
- `not`

The implementation also handles the values `0`, `1`, `x`, and `z` in a limited but practical way. The tri-state gate `bufif1` outputs `z` when its enable input is `0`. Unknown values are propagated conservatively where needed.

---

## 5. Testing
Five original test circuits were created and placed under the `tests/` folder. Each test folder contains:
- a Verilog circuit file
- a stimuli file
- an expected simulation output file
- short hand-simulation notes

### 5.1 Test 1 - Inverter
A single `not` gate verifies basic event propagation and delay handling.

### 5.2 Test 2 - AND/OR Cascade
An `and` feeding an `or` verifies internal wire changes and two-level delay propagation.

### 5.3 Test 3 - XOR/XNOR Chain
This test checks parity logic and verifies that the output changes after both the first and second gate delays.

### 5.4 Test 4 - Tri-State Buffer
A `bufif1` gate verifies enable-controlled output behavior and the correct appearance of `z` in the simulation output.

### 5.5 Test 5 - Mixed Circuit
A larger circuit using `nand`, `nor`, `xor`, and `buf` verifies multiple internal wires and a longer propagation path.

These tests were designed to check both correctness and timing, not only logic values.

---

## 6. Challenges
Several implementation challenges appeared in this project:

### 6.1 Parsing Gate Delays
One of the main issues was correctly parsing Verilog gate lines with delays such as `#(2)`. The parser must separate the primitive type, the delay value, the gate name, and the pin list.

### 6.2 Relative Timing in the Stimuli File
The stimuli file uses delays relative to the previous event, not absolute timestamps. Therefore, the parser has to accumulate the delays while reading the file.

### 6.3 Event Ordering
Two events may have the same timestamp. To keep simulation behavior deterministic, a sequence number is used as a tie-breaker in the priority queue.

### 6.4 Avoiding Unnecessary Events
If a gate output evaluates to the same value it already has, no new event should be written to the output file. Skipping duplicate events avoids clutter and keeps the simulator correct.

---

## 7. Instructions to Build and Use the Program

### 7.1 Build
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### 7.2 Run
```bash
./EventDrivenSimulatorQt circuit.v stimuli.stim output.sim
```

Or run with no arguments to use the sample `example.v` and `example.stim`.

---

## 8. Remaining Issues / Limitations
The current implementation is suitable for the project requirements, but some limitations remain:

- it supports only the primitive gate set requested in the project
- it does not implement full Verilog syntax
- it does not implement advanced four-valued Verilog semantics beyond basic practical handling of `x` and `z`
- the visual waveform viewer is not part of the base version unless added as a bonus feature

---

## 9. Contributions of Each Member

- Member 1: parser implementation, testing, and report writing
- Member 2: event queue, simulator core, bug fixing, and examples
---

## 10. AI Usage Note
AI tools were used to help review design decisions, explain bugs, improve documentation, and generate test scaffolding. All submitted code should still be checked, understood, and explained by the team during the demo. This section should be edited so it reflects your actual usage honestly, including the prompts you used if your instructor requires them.

---

## 11. Conclusion
This project demonstrates how event-driven simulation can model digital circuits efficiently by reacting only to signal changes. The final design combines parsing, data structures, and discrete-event scheduling to generate correct timed simulation outputs for the required Verilog primitive circuits.