# Digital-design-project-1
First group project in digital design 1

Omar Hassan 900231215
Habiba Khashaba 900230033


# Event-Driven Logic Circuits Simulator

## Introduction
Welcome to the Event-Driven Logic Circuits Simulator project! In this project, you'll create a simulator that can model and simulate digital circuits based on events. Using Verilog to describe the circuit, the simulator will react to changes in inputs and update the outputs accordingly. This event-driven approach will help you better understand how digital systems respond to discrete changes over time.

## What's the Goal?
The main goal of this project is to get hands-on experience with structural Verilog descriptions and the operation of logic circuit simulators. You'll be building a tool that processes changes (events) in a digital circuit and updates its state based on those changes.

## Key Files You’ll Work With
- **Verilog File (.v)**: A file where you describe your digital circuit using Verilog. The simulator will support basic logic gates like AND, OR, XOR, and a few others.
- **Stimuli File (.stim)**: This file contains the input events for your circuit. Each event will specify a change in an input at a particular time.
- **Simulation Output File (.sim)**: Once the simulation runs, this file records all the events that occurred in the circuit (changes in inputs, wires, and outputs).

## How It Works

### What You Need to Do:
1. **Verilog Circuit Description**: 
   You'll provide a Verilog file that describes the digital circuit you want to simulate. Your file will use the following Verilog primitives:
   - AND, OR, XOR, NAND, NOR, XNOR, BUF, BUFIF1, and NOT.

2. **Input Events (Stimuli)**:
   The `.stim` file contains the events that happen at the inputs of your circuit. Each event is timestamped and will trigger changes in the circuit. For example:
   ```text
   #0 A=0;
   #500 B=1;
