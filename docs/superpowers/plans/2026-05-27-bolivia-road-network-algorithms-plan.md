# Bolivia Road Network Algorithms Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add five graph algorithms (vehicle reach, connected components, road diameter, MST, route comparison) and an interactive menu to the existing Bolivia road network C++ project.

**Architecture:** Extend the existing `Graph` class with Dijkstra-based algorithms. WCC uses BFS on an undirected view. MST uses Kruskal with union-find on the giant component. Diameter uses a two-sweep heuristic restricted to the giant component. Menu lives in `main.cpp`.

**Tech Stack:** C++17, CMake, standard library only.

---

## File Structure

- `include/graph.h` — Add `ComponentInfo` struct and all new public method declarations.
- `src/graph.cpp` — Implement Dijkstra, WCC, vehicle reach, diameter, MST, route comparison.
- `src/main.cpp` — Replace the sample output with an interactive menu loop.

No new files created. No changes to `csv_parser.{h,cpp}` or `CMakeLists.txt`.

---

### Task 1: Generic Dijkstra

**Files:**
- Modify: `include/graph.h`
- Modify: `src/graph.cpp`

- [ ] **Step 1: Add Dijkstra declaration to `include/graph.h`**

Add these includes at the top of `graph.h` (keep existing ones):
```cpp
#include <limits>
#include <queue>
```

Add the public method declaration inside the `Graph` class, after `edgeCount()`:
```cpp
    // Dijkstra from source. max_distance < 0 means no cutoff.
    // use_time = true minimizes time_weight, false minimizes weight.
    // If predecessor != nullptr, fills it with the previous node on the shortest path.
    std::vector<double> dijkstra(uint32_t source,
                                 double max_distance = -1.0,
                                 bool use_time = false,
                                 std::vector<uint32_t>* predecessor = nullptr) const;
```

- [ ] **Step 2: Implement Dijkstra in `src/graph.cpp`**

Add these includes at the top of `graph.cpp` (keep existing ones):
```cpp
#include <limits>
#include <queue>
```

Add the implementation after `Graph::edgeCount()`:
```cpp
std::vector<double> Graph::dijkstra(uint32_t source,
                                    double max_distance,
                                    bool use_time,
                                    std::vector<uint32_t>* predecessor) const {
    const double INF = std::numeric_limits<double>::infinity();
    size_t n = adj_.size();
    std::vector<double> dist(n, INF);
    if (source >= n) {
        return dist;
    }

    if (predecessor) {
        predecessor->assign(n, std::numeric_limits<uint32_t>::max());
        (*predecessor)[source] = source;
    }

    dist[source] = 0.0;
    using P = std::pair<double, uint32_t>;
    std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
    pq.emplace(0.0, source);

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d > dist[u]) continue;
        if (max_distance >= 0.0 && d > max_distance) continue;

        for (const auto& e : adj_[u]) {
            double w = use_time ? e.time_weight : e.weight;
            if (dist[e.to] > d + w) {
                dist[e.to] = d + w;
                if (predecessor) {
                    (*predecessor)[e.to] = u;
                }
                pq.emplace(dist[e.to], e.to);
            }
        }
    }
    return dist;
}
```

- [ ] **Step 3: Build to verify compilation**

Run:
```bash
cmake -B build
cmake --build build
```

Expected: compilation succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add include/graph.h src/graph.cpp
git commit -m "feat: add generic Dijkstra with cutoff and predecessor tracking"
```

---

### Task 2: Vehicle Reach

**Files:**
- Modify: `include/graph.h`
- Modify: `src/graph.cpp`

- [ ] **Step 1: Add declaration to `include/graph.h`**

Add inside the `Graph` class, after the `dijkstra` declaration:
```cpp
    // Count nodes reachable within max_dist_m street distance from source.
    size_t vehicleReach(uint32_t source, double max_dist_m = 5000.0) const;
```

- [ ] **Step 2: Implement in `src/graph.cpp`**

Add after the `dijkstra` implementation:
```cpp
size_t Graph::vehicleReach(uint32_t source, double max_dist_m) const {
    auto dist = dijkstra(source, max_dist_m, false, nullptr);
    size_t count = 0;
    for (double d : dist) {
        if (d <= max_dist_m) {
            ++count;
        }
    }
    return count;
}
```

- [ ] **Step 3: Build to verify compilation**

Run:
```bash
cmake --build build
```

Expected: compilation succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add include/graph.h src/graph.cpp
git commit -m "feat: add vehicle reach within 5km"
```

---

### Task 3: Weakly Connected Components

**Files:**
- Modify: `include/graph.h`
- Modify: `src/graph.cpp`

- [ ] **Step 1: Add ComponentInfo struct and declaration to `include/graph.h`**

Add before the `Graph` class declaration:
```cpp
struct ComponentInfo {
    std::vector<int> component_id;   // -1 for isolated nodes
    std::vector<size_t> sizes;       // size of each component
    size_t giant_component_idx = 0;  // index of largest component
};
```

Add inside the `Graph` class, after `vehicleReach`:
```cpp
    ComponentInfo weaklyConnectedComponents() const;
```

- [ ] **Step 2: Implement in `src/graph.cpp`**

Add after `vehicleReach`:
```cpp
ComponentInfo Graph::weaklyConnectedComponents() const {
    ComponentInfo info;
    size_t n = adj_.size();
    info.component_id.assign(static_cast<int>(n), -1);
    std::vector<char> visited(n, 0);

    // Build reverse adjacency for undirected traversal
    std::vector<std::vector<uint32_t>> rev(n);
    for (uint32_t u = 0; u < n; ++u) {
        for (const auto& e : adj_[u]) {
            rev[e.to].push_back(u);
        }
    }

    for (uint32_t start = 0; start < n; ++start) {
        if (visited[start]) continue;

        std::vector<uint32_t> stack;
        stack.push_back(start);
        visited[start] = 1;
        size_t comp_size = 0;
        int comp_idx = static_cast<int>(info.sizes.size());

        while (!stack.empty()) {
            uint32_t u = stack.back();
            stack.pop_back();
            info.component_id[u] = comp_idx;
            ++comp_size;

            // Outgoing edges
            for (const auto& e : adj_[u]) {
                if (!visited[e.to]) {
                    visited[e.to] = 1;
                    stack.push_back(e.to);
                }
            }
            // Incoming edges
            for (uint32_t v : rev[u]) {
                if (!visited[v]) {
                    visited[v] = 1;
                    stack.push_back(v);
                }
            }
        }

        info.sizes.push_back(comp_size);
        if (comp_size > info.sizes[info.giant_component_idx]) {
            info.giant_component_idx = info.sizes.size() - 1;
        }
    }

    return info;
}
```

- [ ] **Step 3: Build to verify compilation**

Run:
```bash
cmake --build build
```

Expected: compilation succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add include/graph.h src/graph.cpp
git commit -m "feat: add weakly connected components with giant component tracking"
```

---

### Task 4: Road Diameter (Multi-Sweep Heuristic)

**Files:**
- Modify: `include/graph.h`
- Modify: `src/graph.cpp`

- [ ] **Step 1: Add declaration to `include/graph.h`**

Add inside the `Graph` class, after `weaklyConnectedComponents`:
```cpp
    // Multi-sweep heuristic on the giant component.
    double roadDiameter() const;
```

- [ ] **Step 2: Implement in `src/graph.cpp`**

Add after `weaklyConnectedComponents`:
```cpp
double Graph::roadDiameter() const {
    auto comp = weaklyConnectedComponents();
    size_t n = adj_.size();

    // Find any node in the giant component
    uint32_t start = 0;
    bool found = false;
    int giant_idx = static_cast<int>(comp.giant_component_idx);
    for (uint32_t u = 0; u < n; ++u) {
        if (comp.component_id[u] == giant_idx) {
            start = u;
            found = true;
            break;
        }
    }
    if (!found) return 0.0;

    // First sweep: find farthest node u from start
    auto dist1 = dijkstra(start, -1.0, false, nullptr);
    uint32_t u = start;
    double max_dist = 0.0;
    for (uint32_t v = 0; v < n; ++v) {
        if (comp.component_id[v] == giant_idx && std::isfinite(dist1[v])) {
            if (dist1[v] > max_dist) {
                max_dist = dist1[v];
                u = v;
            }
        }
    }

    // Second sweep: find farthest node from u
    auto dist2 = dijkstra(u, -1.0, false, nullptr);
    double diameter = 0.0;
    for (uint32_t v = 0; v < n; ++v) {
        if (comp.component_id[v] == giant_idx && std::isfinite(dist2[v])) {
            if (dist2[v] > diameter) {
                diameter = dist2[v];
            }
        }
    }

    return diameter;
}
```

- [ ] **Step 3: Build to verify compilation**

Run:
```bash
cmake --build build
```

Expected: compilation succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add include/graph.h src/graph.cpp
git commit -m "feat: add road diameter via multi-sweep heuristic on giant component"
```

---

### Task 5: Minimum Emergency Network (MST via Kruskal)

**Files:**
- Modify: `include/graph.h`
- Modify: `src/graph.cpp`

- [ ] **Step 1: Add declaration to `include/graph.h`**

Add inside the `Graph` class, after `roadDiameter`:
```cpp
    // Build MST on the giant component. Returns total weight in kilometers.
    double minimumSpanningTree() const;
```

- [ ] **Step 2: Add Union-Find helper to `src/graph.cpp`**

Add at the top of `src/graph.cpp`, before `inferSpeed`, in the anonymous namespace or as a file-local struct:
```cpp
namespace {
struct UnionFind {
    std::vector<int> parent;
    explicit UnionFind(size_t n) : parent(n) {
        for (size_t i = 0; i < n; ++i) parent[i] = static_cast<int>(i);
    }
    int find(int x) {
        if (parent[x] != x) parent[x] = find(parent[x]);
        return parent[x];
    }
    bool unite(int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra == rb) return false;
        parent[ra] = rb;
        return true;
    }
};
} // namespace
```

- [ ] **Step 3: Implement MST in `src/graph.cpp`**

Add after `roadDiameter`:
```cpp
double Graph::minimumSpanningTree() const {
    auto comp = weaklyConnectedComponents();
    size_t n = adj_.size();
    int giant_idx = static_cast<int>(comp.giant_component_idx);

    struct UndirEdge {
        uint32_t u, v;
        double weight;
        bool operator<(const UndirEdge& other) const {
            return weight < other.weight;
        }
    };

    std::vector<UndirEdge> undir_edges;
    undir_edges.reserve(n * 2);

    for (uint32_t u = 0; u < n; ++u) {
        if (comp.component_id[u] != giant_idx) continue;
        for (const auto& e : adj_[u]) {
            if (comp.component_id[e.to] != giant_idx) continue;
            uint32_t a = std::min(u, e.to);
            uint32_t b = std::max(u, e.to);
            undir_edges.push_back({a, b, e.weight});
        }
    }

    // Deduplicate parallel undirected edges: keep shortest weight per pair
    std::sort(undir_edges.begin(), undir_edges.end(), [](const UndirEdge& a, const UndirEdge& b) {
        if (a.u != b.u) return a.u < b.u;
        if (a.v != b.v) return a.v < b.v;
        return a.weight < b.weight;
    });

    std::vector<UndirEdge> unique_edges;
    unique_edges.reserve(undir_edges.size());
    for (const auto& e : undir_edges) {
        if (unique_edges.empty() ||
            unique_edges.back().u != e.u ||
            unique_edges.back().v != e.v) {
            unique_edges.push_back(e);
        }
    }

    UnionFind uf(n);
    double total = 0.0;
    size_t edges_used = 0;
    size_t target_edges = comp.sizes[giant_idx] > 0 ? comp.sizes[giant_idx] - 1 : 0;

    for (const auto& e : unique_edges) {
        if (uf.unite(static_cast<int>(e.u), static_cast<int>(e.v))) {
            total += e.weight;
            ++edges_used;
            if (edges_used == target_edges) break;
        }
    }

    return total / 1000.0;
}
```

- [ ] **Step 4: Build to verify compilation**

Run:
```bash
cmake --build build
```

Expected: compilation succeeds with no errors.

- [ ] **Step 5: Commit**

```bash
git add include/graph.h src/graph.cpp
git commit -m "feat: add Kruskal MST on giant component"
```

---

### Task 6: Distance vs. Time Route Comparison

**Files:**
- Modify: `include/graph.h`
- Modify: `src/graph.cpp`

- [ ] **Step 1: Add declaration to `include/graph.h`**

Add inside the `Graph` class, after `minimumSpanningTree`:
```cpp
    // Compare shortest path by distance vs. by time between src and dst.
    void compareRoutes(uint32_t src, uint32_t dst) const;
```

- [ ] **Step 2: Implement in `src/graph.cpp`**

Add these includes at the top of `src/graph.cpp` (keep existing ones):
```cpp
#include <unordered_set>
#include <chrono>
#include <iostream>
#include <iomanip>
```

Add after `minimumSpanningTree`:
```cpp
void Graph::compareRoutes(uint32_t src, uint32_t dst) const {
    if (!hasNode(src) || !hasNode(dst)) {
        std::cout << "Invalid node ID(s)." << std::endl;
        return;
    }

    auto start = std::chrono::steady_clock::now();

    // Distance-optimal
    std::vector<uint32_t> prev_d;
    auto dist_d = dijkstra(src, -1.0, false, &prev_d);

    // Time-optimal
    std::vector<uint32_t> prev_t;
    auto dist_t = dijkstra(src, -1.0, true, &prev_t);

    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    auto reconstruct = [](const std::vector<uint32_t>& prev, uint32_t s, uint32_t t) {
        std::vector<uint32_t> path;
        if (prev.empty() || prev[t] == std::numeric_limits<uint32_t>::max()) {
            return path;
        }
        uint32_t cur = t;
        while (cur != s) {
            path.push_back(cur);
            cur = prev[cur];
        }
        path.push_back(s);
        std::reverse(path.begin(), path.end());
        return path;
    };

    auto path_d = reconstruct(prev_d, src, dst);
    auto path_t = reconstruct(prev_t, src, dst);

    auto pathMetric = [&](const std::vector<uint32_t>& path, bool use_time) -> double {
        double total = 0.0;
        for (size_t i = 0; i + 1 < path.size(); ++i) {
            uint32_t u = path[i];
            uint32_t v = path[i + 1];
            bool found = false;
            for (const auto& e : adj_[u]) {
                if (e.to == v) {
                    total += use_time ? e.time_weight : e.weight;
                    found = true;
                    break;
                }
            }
            if (!found) return std::numeric_limits<double>::infinity();
        }
        return total;
    };

    double total_time_d = pathMetric(path_d, true);
    double total_dist_t = pathMetric(path_t, false);

    std::unordered_set<uint32_t> nodes_d(path_d.begin(), path_d.end());
    size_t overlap = 0;
    for (uint32_t node : path_t) {
        if (nodes_d.count(node)) ++overlap;
    }

    std::cout << "\n--- Distance-optimal route ---\n";
    if (path_d.empty()) {
        std::cout << "No path found.\n";
    } else {
        std::cout << "Distance: " << std::fixed << std::setprecision(2) << dist_d[dst] << " m\n";
        std::cout << "Time: " << std::fixed << std::setprecision(2) << total_time_d << " s\n";
        std::cout << "Edges: " << (path_d.size() - 1) << "\n";
    }

    std::cout << "\n--- Time-optimal route ---\n";
    if (path_t.empty()) {
        std::cout << "No path found.\n";
    } else {
        std::cout << "Distance: " << std::fixed << std::setprecision(2) << total_dist_t << " m\n";
        std::cout << "Time: " << std::fixed << std::setprecision(2) << dist_t[dst] << " s\n";
        std::cout << "Edges: " << (path_t.size() - 1) << "\n";
    }

    std::cout << "\nOverlap (shared nodes): " << overlap << "\n";
    std::cout << "Total computation time: " << ms << " ms\n";
}
```

- [ ] **Step 3: Build to verify compilation**

Run:
```bash
cmake --build build
```

Expected: compilation succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add include/graph.h src/graph.cpp
git commit -m "feat: add distance vs time route comparison"
```

---

### Task 7: Interactive Menu in main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Replace `src/main.cpp` with menu-driven version**

The complete new content of `src/main.cpp`:

```cpp
#include <iostream>
#include <iomanip>
#include <limits>
#include <chrono>
#include "csv_parser.h"
#include "graph.h"

static void printMenu() {
    std::cout << "\n=== Bolivia Road Network Analysis ===\n"
              << "1. Vehicle Reach              — count nodes within 5 km of a source\n"
              << "2. Connected Components       — giant component size & island count\n"
              << "3. Road Diameter              — estimated farthest pair (multi-sweep)\n"
              << "4. Emergency MST              — minimum spanning tree distance (km)\n"
              << "5. Distance vs. Time Route    — compare two shortest paths\n"
              << "6. Exit\n"
              << "Choice: ";
}

int main() {
    try {
        CleansingOptions options;
        options.filter_non_drivable = true;
        options.remove_self_loops = true;
        options.deduplicate_edges = true;

        ParsingStats stats;

        std::cout << "Loading nodes..." << std::endl;
        auto nodes = CSVParser::loadNodes("../data/nodes.csv", options, stats);

        std::cout << "Loading edges..." << std::endl;
        auto edges = CSVParser::loadEdges("../data/edges.csv", nodes, options, stats);

        std::cout << "\n================ DATA CLEANSED STATISTICS ================" << std::endl;
        std::cout << "NODES:" << std::endl;
        std::cout << "  Total lines read:                 " << stats.total_nodes_read << std::endl;
        std::cout << "  Malformed nodes skipped:          " << stats.malformed_nodes_skipped << std::endl;
        std::cout << "  Out-of-bounds nodes skipped:      " << stats.out_of_bounds_nodes_skipped << std::endl;
        std::cout << "  Valid nodes kept:                 " << stats.valid_nodes_kept << std::endl;
        std::cout << "EDGES:" << std::endl;
        std::cout << "  Total lines read:                 " << stats.total_edges_read << std::endl;
        std::cout << "  Malformed edges skipped:          " << stats.malformed_edges_skipped << std::endl;
        std::cout << "  Edges referencing invalid nodes:  " << stats.invalid_node_refs_filtered << std::endl;
        std::cout << "  Self-loop edges removed:          " << stats.self_loops_removed << std::endl;
        std::cout << "  Non-drivable path edges filtered: " << stats.non_drivable_edges_filtered << std::endl;
        std::cout << "  Duplicate edges filtered:         " << stats.duplicate_edges_filtered << std::endl;
        std::cout << "  Valid edges kept:                 " << stats.valid_edges_kept << std::endl;
        std::cout << "==========================================================\n" << std::endl;

        std::cout << "Loaded " << nodes.size() << " nodes and "
                  << edges.size() << " edges from CSV." << std::endl;

        std::cout << "Building graph..." << std::endl;
        auto graph = Graph::build(nodes, edges);
        std::cout << "Graph built: " << graph.nodeCount() << " nodes, "
                  << graph.edgeCount() << " directed edges." << std::endl;

        int choice = 0;
        do {
            printMenu();
            if (!(std::cin >> choice)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Invalid input.\n";
                continue;
            }

            switch (choice) {
                case 1: {
                    std::cout << "Enter source node ID: ";
                    uint32_t src;
                    if (std::cin >> src && graph.hasNode(src)) {
                        auto t0 = std::chrono::steady_clock::now();
                        size_t reach = graph.vehicleReach(src);
                        auto t1 = std::chrono::steady_clock::now();
                        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
                        std::cout << "Nodes reachable within 5 km: " << reach
                                  << " (time: " << ms << " ms)\n";
                    } else {
                        std::cout << "Invalid node ID.\n";
                        std::cin.clear();
                        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    }
                    break;
                }
                case 2: {
                    auto t0 = std::chrono::steady_clock::now();
                    auto comp = graph.weaklyConnectedComponents();
                    auto t1 = std::chrono::steady_clock::now();
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
                    std::cout << "Number of islands (components): " << comp.sizes.size() << "\n";
                    std::cout << "Giant component size: " << comp.sizes[comp.giant_component_idx]
                              << " nodes\n";
                    std::cout << "Time: " << ms << " ms\n";
                    break;
                }
                case 3: {
                    auto t0 = std::chrono::steady_clock::now();
                    double diam = graph.roadDiameter();
                    auto t1 = std::chrono::steady_clock::now();
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
                    std::cout << "Estimated road diameter: " << std::fixed << std::setprecision(2)
                              << diam << " m\n";
                    std::cout << "Time: " << ms << " ms\n";
                    break;
                }
                case 4: {
                    auto t0 = std::chrono::steady_clock::now();
                    double mst_km = graph.minimumSpanningTree();
                    auto t1 = std::chrono::steady_clock::now();
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
                    std::cout << "MST total distance: " << std::fixed << std::setprecision(2)
                              << mst_km << " km\n";
                    std::cout << "Time: " << ms << " ms\n";
                    break;
                }
                case 5: {
                    std::cout << "Enter source node ID: ";
                    uint32_t src;
                    if (!(std::cin >> src) || !graph.hasNode(src)) {
                        std::cout << "Invalid source node ID.\n";
                        std::cin.clear();
                        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                        break;
                    }
                    std::cout << "Enter destination node ID: ";
                    uint32_t dst;
                    if (!(std::cin >> dst) || !graph.hasNode(dst)) {
                        std::cout << "Invalid destination node ID.\n";
                        std::cin.clear();
                        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                        break;
                    }
                    graph.compareRoutes(src, dst);
                    break;
                }
                case 6:
                    std::cout << "Goodbye.\n";
                    break;
                default:
                    std::cout << "Invalid choice.\n";
            }
        } while (choice != 6);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

- [ ] **Step 2: Build to verify compilation**

Run:
```bash
cmake --build build
```

Expected: compilation succeeds with no errors.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: add interactive menu for all algorithms"
```

---

### Task 8: Final Build Verification

**Files:**
- None (verification only)

- [ ] **Step 1: Clean rebuild**

Run:
```bash
rm -rf build
cmake -B build
cmake --build build
```

Expected: clean compilation with zero errors and zero warnings (under `-Wall -Wextra` if enabled).

- [ ] **Step 2: Run executable without data (expect error)**

Run:
```bash
./build/algorithms
```

Expected: program attempts to load `../data/nodes.csv`, fails with "Failed to open file: ../data/nodes.csv" (or similar), exits with code 1. This confirms the binary links and runs correctly.

- [ ] **Step 3: Commit**

```bash
git commit --allow-empty -m "build: verify clean build"
```

---

## Self-Review Checklist

**1. Spec coverage:**
- Vehicle Reach (Dijkstra with 5km cutoff) → Task 2
- Weakly Connected Components (BFS, giant component) → Task 3
- Road Diameter (multi-sweep heuristic) → Task 4
- MST (Kruskal on giant component, km output) → Task 5
- Distance vs. Time comparison (two Dijkstras, path reconstruction) → Task 6
- Interactive menu → Task 7
- Timing output → embedded in Task 7
- **Gap:** none.

**2. Placeholder scan:**
- No "TBD", "TODO", "implement later", or vague descriptions.
- Every step contains exact file paths, exact code blocks, exact commands, and expected output.
- No references to undefined types or methods.

**3. Type consistency:**
- `ComponentInfo` struct defined in Task 3, used in Tasks 4 and 5 — consistent.
- `dijkstra` signature uses `std::vector<uint32_t>* predecessor` — used in Task 6.
- `vehicleReach`, `weaklyConnectedComponents`, `roadDiameter`, `minimumSpanningTree`, `compareRoutes` all declared in `graph.h` and implemented in `graph.cpp`.
- `UnionFind` is a file-local struct in `graph.cpp` — not exposed in header.

**4. Edge cases handled:**
- Invalid source/destination nodes: checked in `compareRoutes`, `vehicleReach`, and `main.cpp`.
- Isolated graph / single-node giant component: diameter returns 0, MST returns 0.
- Source node with no edges: `vehicleReach` returns 1 (the source itself).
- Missing data file: caught by existing `CSVParser::readFile` and `main.cpp` try/catch.

Plan is complete and ready for execution.
