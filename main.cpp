#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

// If CMake doesn't pass the project source directory, fall back to current directory.
// This helps the simulator find the example files when run from different locations.
#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Data Structures
// ─────────────────────────────────────────────────────────────────────────────

// Represents one gate in the circuit (e.g., "and #(2) G0(w1, A, B)").
// pins[0] is always the output; pins[1..] are the inputs.
// This maps directly to how structural Verilog primitives are written.
struct Gate {
    string type;              // Gate kind: and, or, xor, not, bufif1, etc.
    string name;              // Instance name: G0, G1, etc.
    long long delay_ps = 1;   // How many picoseconds before the output reacts to an input change
    vector<string> pins;      // pins[0] = output signal, pins[1..] = input signals
};

// Represents one event in the simulation — a signal changing its value at a specific time.
// We store a sequence number so that two events at the same timestamp always
// process in a consistent order, making the output deterministic across runs.
struct Event {
    long long time_ps;   // Absolute simulation time when this change happens
    string signal;       // Which signal is changing
    char value;          // New value: '0', '1', 'x' (unknown), or 'z' (high-impedance)
    long long seq;       // Tie-breaker: lower seq = processed first at same timestamp
};

// Custom comparator that makes std::priority_queue behave as a min-heap.
// The earliest event (smallest time_ps) always comes out first.
// If two events share the same timestamp, the one scheduled earlier (lower seq) wins.
struct EventCompare {
    bool operator()(const Event& a, const Event& b) const {
        if (a.time_ps != b.time_ps) return a.time_ps > b.time_ps;
        return a.seq > b.seq;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// String Utilities
// ─────────────────────────────────────────────────────────────────────────────

// Strips leading and trailing whitespace from a string.
static string trim(const string& s) {
    size_t i = 0;
    while (i < s.size() && isspace(static_cast<unsigned char>(s[i]))) ++i;

    size_t j = s.size();
    while (j > i && isspace(static_cast<unsigned char>(s[j - 1]))) --j;

    return s.substr(i, j - i);
}

// Returns true if string s begins with the given prefix.
static bool startsWith(const string& s, const string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

// Removes everything after // on a line (inline Verilog-style comments).
static void stripComment(string& line) {
    size_t pos = line.find("//");
    if (pos != string::npos) {
        line = line.substr(0, pos);
    }
}

// Splits a comma-separated string like "w1, A, B" into a vector {"w1", "A", "B"}.
static vector<string> splitCSV(const string& s) {
    vector<string> out;
    string token;
    stringstream ss(s);

    while (getline(ss, token, ',')) {
        token = trim(token);
        if (!token.empty()) out.push_back(token);
    }
    return out;
}

// Converts a logic value character to lowercase and validates it.
// Accepts '0', '1', 'x'/'X' (unknown), 'z'/'Z' (high-impedance).
// Throws if the character is anything else.
static char normalizeLogic(char c) {
    c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (c == '0' || c == '1' || c == 'x' || c == 'z') return c;
    throw runtime_error(string("Invalid logic value: ") + c);
}

// ─────────────────────────────────────────────────────────────────────────────
// Simulator Class
// ─────────────────────────────────────────────────────────────────────────────
class Simulator {
public:

    // Reads and parses a structural Verilog file.
    // Extracts all input/output/wire declarations and gate instantiations.
    // After loading, every declared signal is set to 'x' (unknown) so that
    // any time-0 stimulus that assigns a known value will register as a real change.
    void loadCircuit(const fs::path& filename) {
        ifstream fin(filename);
        if (!fin) {
            throw runtime_error("Cannot open circuit file: " + filename.string());
        }

        string line;
        while (getline(fin, line)) {
            stripComment(line);
            line = trim(line);
            if (line.empty()) continue;

            // Skip the module header and endmodule — we only care about what's inside.
            if (startsWith(line, "module") || startsWith(line, "endmodule")) {
                continue;
            }

            if (startsWith(line, "input")) {
                parseDeclaration(line, inputs_);
            } else if (startsWith(line, "output")) {
                parseDeclaration(line, outputs_);
            } else if (startsWith(line, "wire")) {
                parseDeclaration(line, wires_);
            } else {
                parseGate(line);
            }
        }

        // Initialize all declared signals to 'x'.
        // This is important: if we left them uninitialized, time-0 stimuli that
        // set a signal to '0' might not be recorded because '0' == default.
        // Starting at 'x' guarantees every first assignment is captured.
        for (const auto& s : inputs_)  signals_[s] = 'x';
        for (const auto& s : outputs_) signals_[s] = 'x';
        for (const auto& s : wires_)   signals_[s] = 'x';
    }

    // Reads the stimuli file and schedules all input events onto the priority queue.
    // Each delay in the file is RELATIVE to the previous event (not absolute),
    // so we accumulate a running total to get the absolute timestamp for each event.
    void loadStimuli(const fs::path& filename) {
        ifstream fin(filename);
        if (!fin) {
            throw runtime_error("Cannot open stimuli file: " + filename.string());
        }

        string line;
        long long current_time = 0;

        while (getline(fin, line)) {
            stripComment(line);
            line = trim(line);
            if (line.empty()) continue;

            if (line.back() != ';') {
                throw runtime_error("Stimulus line must end with ';': " + line);
            }

            line.pop_back();
            line = trim(line);

            if (line.empty() || line[0] != '#') {
                throw runtime_error("Stimulus line must start with '#': " + line);
            }

            // Split "#100 A=1" into delay "100" and assignment "A=1".
            size_t space_pos = line.find_first_of(" \t");
            if (space_pos == string::npos) {
                throw runtime_error("Invalid stimulus format: " + line);
            }

            string delay_str = line.substr(1, space_pos - 1);
            string assign = trim(line.substr(space_pos + 1));

            // Add the relative delay to our running clock.
            long long delta = stoll(delay_str);
            current_time += delta;

            size_t eq_pos = assign.find('=');
            if (eq_pos == string::npos) {
                throw runtime_error("Stimulus assignment missing '=': " + assign);
            }

            string sig = trim(assign.substr(0, eq_pos));
            string val = trim(assign.substr(eq_pos + 1));

            if (val.size() != 1) {
                throw runtime_error("Stimulus value must be one character: " + assign);
            }

            char logic = normalizeLogic(val[0]);

            // Only allow signals that were declared in the circuit file.
            if (!signals_.count(sig)) {
                throw runtime_error("Stimulus references unknown signal: " + sig);
            }

            schedule(current_time, sig, logic);
        }
    }

    // Runs the simulation and writes every signal transition to the output file.
    // The loop pulls the earliest event from the priority queue, applies the change,
    // then re-evaluates only the gates that depend on the changed signal (via the
    // fanout map). New gate output events are scheduled with the gate's delay added.
    void run(const fs::path& outfile) {
        ofstream fout(outfile);
        if (!fout) {
            throw runtime_error("Cannot open output file: " + outfile.string());
        }

        while (!events_.empty()) {
            Event ev = events_.top();
            events_.pop();

            char old_val = valueOf(ev.signal);

            // If the signal already holds this value, skip the event.
            // This handles reconvergent paths where two different propagation
            // chains schedule the same output change — we only record it once.
            if (old_val == ev.value) continue;

            // Apply the change and write it to the .sim output file.
            signals_[ev.signal] = ev.value;
            fout << ev.time_ps << ", " << ev.signal << ", " << ev.value << "\n";

            // Look up which gates read this signal, then re-evaluate each of them.
            auto it = fanout_.find(ev.signal);
            if (it == fanout_.end()) continue;

            for (int gate_idx : it->second) {
                const Gate& g = gates_[gate_idx];
                char new_out = evalGate(g);
                const string& out_sig = g.pins[0];

                // Only schedule a new event if the gate output actually changed.
                // Scheduling when there's no change would create ghost events.
                if (new_out != valueOf(out_sig)) {
                    schedule(ev.time_ps + g.delay_ps, out_sig, new_out);
                }
            }
        }
    }

private:
    // ── Circuit data ────────────────────────────────────────────────────────
    vector<Gate>                        gates_;    // All gate instances in the circuit
    unordered_set<string>               inputs_;   // Declared input signals
    unordered_set<string>               outputs_;  // Declared output signals
    unordered_set<string>               wires_;    // Declared internal wires
    unordered_map<string, char>         signals_;  // Current logic value of every signal
    unordered_map<string, vector<int>>  fanout_;   // Signal -> indices of gates that read it

    // ── Event queue ─────────────────────────────────────────────────────────
    priority_queue<Event, vector<Event>, EventCompare> events_;
    long long seq_counter_ = 0;  // Increments with every scheduled event for tie-breaking

    // Returns the current value of a signal, defaulting to 'x' if not yet seen.
    char valueOf(const string& signal) const {
        auto it = signals_.find(signal);
        if (it == signals_.end()) return 'x';
        return it->second;
    }

    // Parses a declaration like "input A, B, C;" and inserts each name into dest.
    void parseDeclaration(const string& line, unordered_set<string>& dest) {
        size_t first_space = line.find(' ');
        if (first_space == string::npos) {
            throw runtime_error("Invalid declaration: " + line);
        }

        string rest = trim(line.substr(first_space + 1));
        if (!rest.empty() && rest.back() == ';') {
            rest.pop_back();
        }

        for (const auto& name : splitCSV(rest)) {
            dest.insert(name);
        }
    }

    // Parses a gate line like "and #(2) G0(w1, A, B);" and stores the gate.
    // Also registers each input pin in the fanout map so we know which gates
    // to re-evaluate when a signal changes.
    void parseGate(const string& line) {
        string text = trim(line);
        if (text.empty() || text.back() != ';') {
            throw runtime_error("Gate line must end with ';': " + line);
        }

        text.pop_back();
        text = trim(text);

        size_t first_space = text.find_first_of(" \t");
        if (first_space == string::npos) {
            throw runtime_error("Invalid gate syntax: " + line);
        }

        Gate g;
        g.type = trim(text.substr(0, first_space));
        string rest = trim(text.substr(first_space + 1));

        // Only these nine primitives are supported per the project spec.
        static const unordered_set<string> supported = {
            "and", "or", "xor", "nand", "nor", "xnor", "buf", "bufif1", "not"
        };

        if (!supported.count(g.type)) {
            throw runtime_error("Unsupported gate type: " + g.type);
        }

        // Parse the optional propagation delay: #(2) or #2
        if (startsWith(rest, "#")) {
            size_t open = rest.find('(');
            size_t close = rest.find(')');
            if (open == string::npos || close == string::npos || close < open) {
                throw runtime_error("Invalid delay syntax in gate: " + line);
            }

            string delay_text = trim(rest.substr(open + 1, close - open - 1));
            if (delay_text.empty()) {
                throw runtime_error("Empty delay in gate: " + line);
            }

            g.delay_ps = stoll(delay_text);
            rest = trim(rest.substr(close + 1));
        }

        // Parse the instance name and pin list: G0(w1, A, B)
        size_t left_paren = rest.find('(');
        size_t right_paren = rest.rfind(')');
        if (left_paren == string::npos || right_paren == string::npos || right_paren < left_paren) {
            throw runtime_error("Invalid gate connection syntax: " + line);
        }

        g.name = trim(rest.substr(0, left_paren));
        if (g.name.empty()) {
            throw runtime_error("Gate name missing: " + line);
        }

        string pin_text = rest.substr(left_paren + 1, right_paren - left_paren - 1);
        g.pins = splitCSV(pin_text);

        if (g.pins.size() < 2) {
            throw runtime_error("Gate must have output and at least one input: " + line);
        }

        // buf and not are single-input gates — validate pin count.
        if (g.type == "buf" || g.type == "not") {
            if (g.pins.size() != 2) {
                throw runtime_error(g.type + " requires exactly 1 input: " + line);
            }
        }

        // bufif1 needs exactly: output, data input, enable input.
        if (g.type == "bufif1") {
            if (g.pins.size() != 3) {
                throw runtime_error("bufif1 requires output, data, enable: " + line);
            }
        }

        int idx = static_cast<int>(gates_.size());
        gates_.push_back(g);

        // Make sure the output pin has an entry in the signal table.
        if (!signals_.count(g.pins[0])) signals_[g.pins[0]] = 'x';

        // Register each input pin in the fanout map and the signal table.
        for (size_t i = 1; i < g.pins.size(); ++i) {
            if (!signals_.count(g.pins[i])) signals_[g.pins[i]] = 'x';
            fanout_[g.pins[i]].push_back(idx);
        }
    }

    // Pushes a new event onto the priority queue with the next sequence number.
    void schedule(long long time_ps, const string& signal, char value) {
        events_.push(Event{time_ps, signal, value, seq_counter_++});
    }

    // ── Logic evaluation ─────────────────────────────────────────────────────

    // Returns true only for definite 0 or 1 — not x or z.
    // Used to prevent inverting an unknown value in NAND/NOR/XNOR.
    static bool isKnown01(char c) {
        return c == '0' || c == '1';
    }

    static char logicNot(char a) {
        if (a == '0') return '1';
        if (a == '1') return '0';
        return 'x';  // unknown stays unknown
    }

    // AND: any 0 forces output to 0; all 1s gives 1; anything else is unknown.
    static char logicAnd(const vector<char>& in) {
        bool all_one = true;
        for (char c : in) {
            if (c == '0') return '0';
            if (c != '1') all_one = false;
        }
        return all_one ? '1' : 'x';
    }

    // OR: any 1 forces output to 1; all 0s gives 0; anything else is unknown.
    static char logicOr(const vector<char>& in) {
        bool all_zero = true;
        for (char c : in) {
            if (c == '1') return '1';
            if (c != '0') all_zero = false;
        }
        return all_zero ? '0' : 'x';
    }

    // XOR: counts the parity of 1-bits across all inputs.
    // Returns 'x' if any input is unknown — we can't determine parity.
    static char logicXor(const vector<char>& in) {
        int parity = 0;
        for (char c : in) {
            if (!isKnown01(c)) return 'x';
            parity ^= (c == '1');
        }
        return parity ? '1' : '0';
    }

    // Evaluates a gate by reading its current input values and applying gate logic.
    // NAND/NOR/XNOR use isKnown01() before inverting to avoid turning 'x' into
    // a wrong binary value.
    char evalGate(const Gate& g) const {
        vector<char> in;
        for (size_t i = 1; i < g.pins.size(); ++i) {
            in.push_back(valueOf(g.pins[i]));
        }

        if (g.type == "and")  return logicAnd(in);
        if (g.type == "or")   return logicOr(in);
        if (g.type == "xor")  return logicXor(in);

        if (g.type == "nand") {
            char v = logicAnd(in);
            return isKnown01(v) ? logicNot(v) : 'x';
        }
        if (g.type == "nor") {
            char v = logicOr(in);
            return isKnown01(v) ? logicNot(v) : 'x';
        }
        if (g.type == "xnor") {
            char v = logicXor(in);
            return isKnown01(v) ? logicNot(v) : 'x';
        }

        if (g.type == "buf") return in[0];
        if (g.type == "not") return logicNot(in[0]);

        if (g.type == "bufif1") {
            char data   = in[0];
            char enable = in[1];
            // enable=1 → pass data through
            // enable=0 → output is high-impedance (disconnected)
            // enable=x → we can't determine the state
            if (enable == '1') return data;
            if (enable == '0') return 'z';
            return 'x';
        }

        throw runtime_error("Unknown gate type: " + g.type);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// File Path Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Builds a list of directories to search for input files.
// We check the current directory, the directory containing the executable,
// parent folders (for macOS app bundles), and the CMake source directory.
static vector<fs::path> candidateDirectories(const char* argv0) {
    vector<fs::path> dirs;
    dirs.push_back(fs::current_path());

    try {
        fs::path exe = fs::weakly_canonical(fs::path(argv0));
        fs::path exeDir = exe.parent_path();
        dirs.push_back(exeDir);
        dirs.push_back(exeDir / "..");
        dirs.push_back(exeDir / "../Resources");
        dirs.push_back(exeDir / "../../..");
    } catch (...) {
    }

    dirs.push_back(fs::path(PROJECT_SOURCE_DIR));
    dirs.push_back(fs::path(PROJECT_SOURCE_DIR) / "..");
    return dirs;
}

// Searches multiple candidate directories for a file by name.
// This lets the simulator find input files whether you run it from
// the build folder, the source folder, or inside a macOS app bundle.
static fs::path findExistingFile(const string& name, const char* argv0) {
    fs::path given(name);

    if (given.is_absolute() && fs::exists(given)) return given;
    if (fs::exists(given)) return fs::absolute(given);

    for (const auto& dir : candidateDirectories(argv0)) {
        fs::path candidate = dir / name;
        if (fs::exists(candidate)) return fs::weakly_canonical(candidate);
    }

    throw runtime_error(
        "Could not find file: " + name +
        "\nChecked current directory, executable directory/app bundle, and project source directory."
    );
}

// Chooses a writable location for the output file, trying each candidate
// directory until one accepts a write. Falls back to the current directory.
static fs::path defaultOutputPath(const string& name, const char* argv0) {
    fs::path out(name);
    if (out.is_absolute()) return out;

    for (const auto& dir : candidateDirectories(argv0)) {
        error_code ec;
        if (fs::exists(dir, ec) && fs::is_directory(dir, ec)) {
            fs::path candidate = dir / name;
            ofstream test(candidate, ios::app);
            if (test) {
                test.close();
                return candidate;
            }
        }
    }

    return fs::absolute(out);
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry Point
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // Default to the bundled example files if no arguments are given.
    string circuitName = "example.v";
    string stimuliName = "example.stim";
    string outputName  = "example.sim";

    if (argc == 4) {
        // Use files provided on the command line.
        circuitName = argv[1];
        stimuliName = argv[2];
        outputName  = argv[3];
    } else if (argc != 1) {
        cerr << "Usage: " << argv[0] << " <circuit.v> <stimuli.stim> <output.sim>\n";
        cerr << "Or run with no arguments to use the bundled example files.\n";
        return 1;
    }

    try {
        fs::path circuitPath = findExistingFile(circuitName, argv[0]);
        fs::path stimuliPath = findExistingFile(stimuliName, argv[0]);
        fs::path outputPath  = defaultOutputPath(outputName, argv[0]);

        cout << "Circuit file : " << circuitPath << "\n";
        cout << "Stimuli file : " << stimuliPath << "\n";
        cout << "Output file  : " << outputPath << "\n";

        Simulator sim;
        sim.loadCircuit(circuitPath);
        sim.loadStimuli(stimuliPath);
        sim.run(outputPath);

        cout << "Simulation completed successfully.\n";
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
