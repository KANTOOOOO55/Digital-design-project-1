#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

struct Gate {
    string type;
    string name;
    vector<string> pins; // pins[0] = output, rest = inputs
};

struct Event {
    long long time_ps;
    string signal;
    char value;
    long long seq;
};

struct EventCompare {
    bool operator()(const Event &a, const Event &b) const {
        if (a.time_ps != b.time_ps) return a.time_ps > b.time_ps;
        return a.seq > b.seq;
    }
};

static string trim(const string &s) {
    size_t i = 0;
    while (i < s.size() && isspace(static_cast<unsigned char>(s[i]))) ++i;
    size_t j = s.size();
    while (j > i && isspace(static_cast<unsigned char>(s[j - 1]))) --j;
    return s.substr(i, j - i);
}

static bool startsWith(const string &s, const string &prefix) {
    return s.rfind(prefix, 0) == 0;
}

static void stripComment(string &line) {
    size_t pos = line.find("//");
    if (pos != string::npos) line = line.substr(0, pos);
}

static vector<string> splitCSV(const string &s) {
    vector<string> out;
    string token;
    stringstream ss(s);
    while (getline(ss, token, ',')) {
        token = trim(token);
        if (!token.empty()) out.push_back(token);
    }
    return out;
}

static char normalizeLogic(char c) {
    if (c == 'Z') return 'z';
    return c;
}

class Simulator {
public:
    void loadCircuit(const fs::path &filename) {
        ifstream fin(filename);
        if (!fin) throw runtime_error("Cannot open circuit file: " + filename.string());

        string line;
        while (getline(fin, line)) {
            stripComment(line);
            line = trim(line);
            if (line.empty()) continue;

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

        for (const auto &s : inputs_) signals_[s] = '0';
        for (const auto &s : outputs_) signals_[s] = '0';
        for (const auto &s : wires_) signals_[s] = '0';
    }

    void loadStimuli(const fs::path &filename) {
        ifstream fin(filename);
        if (!fin) throw runtime_error("Cannot open stimuli file: " + filename.string());

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

            size_t space_pos = line.find_first_of(" \t");
            if (space_pos == string::npos) {
                throw runtime_error("Invalid stimulus format: " + line);
            }

            string delay_str = line.substr(1, space_pos - 1);
            string assign = trim(line.substr(space_pos + 1));
            long long delta = stoll(delay_str);
            current_time += delta;

            size_t eq_pos = assign.find('=');
            if (eq_pos == string::npos) {
                throw runtime_error("Stimulus assignment missing '=': " + assign);
            }

            string sig = trim(assign.substr(0, eq_pos));
            string val = trim(assign.substr(eq_pos + 1));
            if (val.size() != 1 || (val[0] != '0' && val[0] != '1' && val[0] != 'z' && val[0] != 'Z')) {
                throw runtime_error("Stimulus value must be 0, 1, or z: " + assign);
            }

            if (!signals_.count(sig)) signals_[sig] = '0';
            schedule(current_time, sig, normalizeLogic(val[0]));
        }
    }

    void run(const fs::path &outfile) {
        ofstream fout(outfile);
        if (!fout) throw runtime_error("Cannot open output file: " + outfile.string());

        while (!events_.empty()) {
            Event ev = events_.top();
            events_.pop();

            char old_val = valueOf(ev.signal);
            if (old_val == ev.value) continue;

            signals_[ev.signal] = ev.value;
            fout << ev.time_ps << ", " << ev.signal << ", " << ev.value << "\n";

            auto it = fanout_.find(ev.signal);
            if (it == fanout_.end()) continue;

            for (int gate_idx : it->second) {
                const Gate &g = gates_[gate_idx];
                char new_out = evalGate(g);
                const string &out_sig = g.pins[0];
                if (new_out != valueOf(out_sig)) {
                    schedule(ev.time_ps + gateDelay(g.type), out_sig, new_out);
                }
            }
        }
    }

private:
    vector<Gate> gates_;
    unordered_set<string> inputs_, outputs_, wires_;
    unordered_map<string, char> signals_;
    unordered_map<string, vector<int>> fanout_;
    priority_queue<Event, vector<Event>, EventCompare> events_;
    long long seq_counter_ = 0;

    char valueOf(const string &signal) const {
        auto it = signals_.find(signal);
        return (it == signals_.end()) ? '0' : it->second;
    }

    void parseDeclaration(const string &line, unordered_set<string> &dest) {
        size_t space_pos = line.find(' ');
        if (space_pos == string::npos) throw runtime_error("Invalid declaration: " + line);
        string rest = trim(line.substr(space_pos + 1));
        if (!rest.empty() && rest.back() == ';') rest.pop_back();
        for (const auto &name : splitCSV(rest)) dest.insert(name);
    }

    void parseGate(const string &line) {
        string text = line;
        if (text.empty() || text.back() != ';') {
            throw runtime_error("Gate line must end with ';': " + line);
        }
        text.pop_back();
        text = trim(text);

        size_t first_space = text.find(' ');
        size_t left_paren = text.find('(');
        size_t right_paren = text.rfind(')');
        if (first_space == string::npos || left_paren == string::npos || right_paren == string::npos || right_paren < left_paren) {
            throw runtime_error("Invalid gate syntax: " + line);
        }

        Gate g;
        g.type = trim(text.substr(0, first_space));
        g.name = trim(text.substr(first_space + 1, left_paren - first_space - 1));
        string pin_text = text.substr(left_paren + 1, right_paren - left_paren - 1);
        g.pins = splitCSV(pin_text);

        if (g.pins.size() < 2) {
            throw runtime_error("Gate must have output and at least one input: " + line);
        }

        static const unordered_set<string> supported = {
            "and", "or", "xor", "nand", "nor", "xnor", "buf", "bufif1", "not"
        };
        if (!supported.count(g.type)) {
            throw runtime_error("Unsupported gate type: " + g.type);
        }

        int idx = static_cast<int>(gates_.size());
        gates_.push_back(g);
        for (size_t i = 1; i < g.pins.size(); ++i) {
            fanout_[g.pins[i]].push_back(idx);
            if (!signals_.count(g.pins[i])) signals_[g.pins[i]] = '0';
        }
        if (!signals_.count(g.pins[0])) signals_[g.pins[0]] = '0';
    }

    void schedule(long long time_ps, const string &signal, char value) {
        events_.push(Event{time_ps, signal, value, seq_counter_++});
    }

    static long long gateDelay(const string &) {
        return 1;
    }

    char evalGate(const Gate &g) const {
        vector<char> in;
        for (size_t i = 1; i < g.pins.size(); ++i) in.push_back(valueOf(g.pins[i]));

        auto all1 = [&]() {
            for (char c : in) if (c != '1') return false;
            return true;
        };
        auto any1 = [&]() {
            for (char c : in) if (c == '1') return true;
            return false;
        };
        auto xorv = [&]() {
            int acc = 0;
            for (char c : in) acc ^= (c == '1');
            return acc ? '1' : '0';
        };

        if (g.type == "and") return all1() ? '1' : '0';
        if (g.type == "or") return any1() ? '1' : '0';
        if (g.type == "xor") return xorv();
        if (g.type == "nand") return all1() ? '0' : '1';
        if (g.type == "nor") return any1() ? '0' : '1';
        if (g.type == "xnor") return xorv() == '1' ? '0' : '1';
        if (g.type == "buf") return in[0];
        if (g.type == "not") {
            if (in[0] == '1') return '0';
            if (in[0] == '0') return '1';
            return 'x';
        }
        if (g.type == "bufif1") {
            if (in.size() != 2) throw runtime_error("bufif1 requires exactly 2 inputs: data, enable");
            return (in[1] == '1') ? in[0] : 'z';
        }
        throw runtime_error("Unknown gate type: " + g.type);
    }
};

static vector<fs::path> candidateDirectories(const char *argv0) {
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

static fs::path findExistingFile(const string &name, const char *argv0) {
    fs::path given(name);
    if (given.is_absolute() && fs::exists(given)) return given;
    if (fs::exists(given)) return fs::absolute(given);

    for (const auto &dir : candidateDirectories(argv0)) {
        fs::path candidate = dir / name;
        if (fs::exists(candidate)) return fs::weakly_canonical(candidate);
    }
    throw runtime_error("Could not find file: " + name + "\nChecked current directory, executable directory/app bundle, and project source directory.");
}

static fs::path defaultOutputPath(const string &name, const char *argv0) {
    fs::path out(name);
    if (out.is_absolute()) return out;

    for (const auto &dir : candidateDirectories(argv0)) {
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

int main(int argc, char *argv[]) {
    string circuitName = "example.v";
    string stimuliName = "example.stim";
    string outputName = "example.sim";

    if (argc == 4) {
        circuitName = argv[1];
        stimuliName = argv[2];
        outputName = argv[3];
    } else if (argc != 1) {
        cerr << "Usage: " << argv[0] << " <circuit.v> <stimuli.stim> <output.sim>\n";
        cerr << "Or run with no arguments to use the bundled example files.\n";
        return 1;
    }

    try {
        fs::path circuitPath = findExistingFile(circuitName, argv[0]);
        fs::path stimuliPath = findExistingFile(stimuliName, argv[0]);
        fs::path outputPath = defaultOutputPath(outputName, argv[0]);

        cout << "Circuit file : " << circuitPath << "\n";
        cout << "Stimuli file : " << stimuliPath << "\n";
        cout << "Output file  : " << outputPath << "\n";

        Simulator sim;
        sim.loadCircuit(circuitPath);
        sim.loadStimuli(stimuliPath);
        sim.run(outputPath);

        cout << "Simulation completed successfully.\n";
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
