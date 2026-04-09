# EventDrivenSimulatorQt (macOS/Qt Creator friendly)

Open `CMakeLists.txt` in Qt Creator.

## Why this version works better on macOS
Qt Creator on macOS often launches CMake apps from inside an `.app` bundle, so relative paths like `example.v` may not be found where you expect. This version fixes that by:

- copying `example.v` and `example.stim` next to the built executable
- copying them into the app bundle Resources folder on macOS
- searching several likely runtime locations automatically
- allowing no-argument runs for Qt Creator

## Run in Qt Creator
Just build and press Run.

## Run from terminal
./EventDrivenSimulatorQt example.v example.stim example.sim

## Output
The simulator writes `example.sim` near the executable or in the current working directory.
