#include <algorithm>
#include <cstdio>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

static const int THROUGHPUT_WINDOW = 10;
static const int QUANTUM = 1;

struct Process {
    string name;
    int burst;
    int arrival;
};

using Timeline = vector<int>;

vector<Process> readProcesses(const string &path) {
    ifstream in(path);
    if (!in) {
        cerr << "Error: cannot open file '" << path << "'\n";
        exit(1);
    }
    vector<Process> procs;
    string line;
    while (getline(in, line)) {
        istringstream ss(line);
        Process p;
        if (!(ss >> p.name)) continue;  // blank line
        if (!(ss >> p.burst >> p.arrival) || p.burst <= 0 || p.arrival < 0) {
            cerr << "Error: bad line: '" << line << "'\n";
            exit(1);
        }
        procs.push_back(p);
    }
    if (procs.size() < 3 || procs.size() > 5)
        cerr << "Warning: expected 3-5 processes, found " << procs.size() << ".\n";
    if (procs.empty()) exit(1);
    return procs;
}


Timeline fifo(const vector<Process> &procs) {
    vector<int> order(procs.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return procs[a].arrival < procs[b].arrival;
    });

    Timeline tl;
    for (int i : order) {
        while ((int)tl.size() < procs[i].arrival) tl.push_back(-1);  // idle
        for (int k = 0; k < procs[i].burst; k++) tl.push_back(i);
    }
    return tl;
}


Timeline sjf(const vector<Process> &procs) {
    int n = procs.size();
    vector<int> rem(n);
    for (int i = 0; i < n; i++) rem[i] = procs[i].burst;

    Timeline tl;
    int current = -1, left = n;
    for (int t = 0; left > 0; t++) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (procs[i].arrival > t || rem[i] == 0) continue;
            if (best == -1 || rem[i] < rem[best] ||
                (rem[i] == rem[best] && i != current && best != current &&
                 procs[i].arrival < procs[best].arrival))
                best = i;
        }
        // Tie with the running process: don't switch.
        if (best != -1 && current != -1 && rem[current] > 0 &&
            procs[current].arrival <= t && rem[current] == rem[best])
            best = current;

        current = best;
        tl.push_back(current);
        if (current != -1 && --rem[current] == 0) left--;
    }
    return tl;
}


Timeline roundRobin(const vector<Process> &procs, int q) {
    int n = procs.size();
    vector<int> rem(n), pending(n);
    for (int i = 0; i < n; i++) { rem[i] = procs[i].burst; pending[i] = i; }
    stable_sort(pending.begin(), pending.end(), [&](int a, int b) {
        return procs[a].arrival < procs[b].arrival;
    });

    deque<int> ready;
    size_t next = 0;  // index into pending
    Timeline tl;
    int t = 0;

    auto admit = [&](int now) {
        while (next < pending.size() && procs[pending[next]].arrival <= now)
            ready.push_back(pending[next++]);
    };

    admit(0);
    while (!ready.empty() || next < pending.size()) {
        if (ready.empty()) {  // idle until next arrival
            tl.push_back(-1);
            admit(++t);
            continue;
        }
        int i = ready.front();
        ready.pop_front();
        int run = min(q, rem[i]);
        for (int k = 0; k < run; k++) { tl.push_back(i); t++; }
        rem[i] -= run;
        admit(t);                       // new arrivals queue before the preempted process
        if (rem[i] > 0) ready.push_back(i);
    }
    return tl;
}


void report(const string &title, const vector<Process> &procs, const Timeline &tl) {
    int n = procs.size();
    size_t width = 0;
    for (auto &p : procs) width = max(width, p.name.size());

    vector<int> first(n, -1), finish(n, 0);
    for (int t = 0; t < (int)tl.size(); t++) {
        int i = tl[t];
        if (i < 0) continue;
        if (first[i] < 0) first[i] = t;
        finish[i] = t + 1;
    }

    cout << "=== " << title << " ===\n";
    for (int i = 0; i < n; i++) {
        string row;
        for (int t = 0; t < (int)tl.size(); t++) {
            if (t < procs[i].arrival || t >= finish[i]) row += ' ';  // not arrived / done
            else row += (tl[t] == i) ? '#' : '_';
        }
        cout << left << setw(width) << procs[i].name << ' ' << row << '\n';
    }

    double totalWait = 0, totalResp = 0;
    int completed = 0;
    for (int i = 0; i < n; i++) {
        totalWait += finish[i] - procs[i].arrival - procs[i].burst;
        totalResp += first[i] - procs[i].arrival;
        if (finish[i] <= THROUGHPUT_WINDOW) completed++;
    }

    cout << fixed << setprecision(2) << '\n'
         << "Average waiting time:  " << totalWait / n << '\n'
         << "Average response time: " << totalResp / n << '\n'
         << "Throughput over " << THROUGHPUT_WINDOW << " cycles: " << completed
         << " process(es) completed (" << (double)completed / THROUGHPUT_WINDOW
         << " per cycle)\n\n";
}

int main(int argc, char *argv[]) {
    string path = (argc > 1) ? argv[1] : "processes.txt";
    vector<Process> procs = readProcesses(path);

    report("FIFO", procs, fifo(procs));
    report("SJF (preemptive)", procs, sjf(procs));
    report("Round Robin (q=1)", procs, roundRobin(procs, QUANTUM));
    return 0;
}
