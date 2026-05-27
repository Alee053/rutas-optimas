# Design Spec: Rutas Óptimas en Red Vial Urbana — Algorithm Implementation

**Date:** 2026-05-27
**Project:** algorithms (C++ Bolivia Road Network)
**Goal:** Add all required graph algorithms and a test menu to `main.cpp`

---

## 1. Context

The project already parses `nodes.csv` and `edges.csv`, cleans the data (filter non-drivable, remove self-loops, deduplicate), and builds a directed weighted `Graph` with:
- `weight` = distance in meters
- `time_weight` = estimated travel time in seconds
- `oneway` flag (reverse edge added only if false)

The graph is an adjacency list of `Edge { to, weight, time_weight, maxspeed, oneway }`.

The missing pieces are the five required analyses from the assignment.

---

## 2. Algorithms

### 2.1 Vehicle Reach

**Requirement:** Given a source node, count how many nodes are reachable in at most 5 km of **street distance** (not Euclidean).

**Algorithm:** Dijkstra with early termination.
- Priority queue ordered by current distance.
- Stop exploring when `dist > 5000 m`.
- Count nodes whose final `dist ≤ 5000 m`.

**Why:** Road segment lengths are strictly positive, so Dijkstra guarantees shortest-path distances. Early termination means we only explore the local neighborhood, making this effectively sublinear on an 886k-node graph.

**Complexity:** O(|E_reachable| log |V_reachable|).

**Edge cases:** Source node may be isolated or out of range → return 1 (itself) or 0.

---

### 2.2 Weakly Connected Components (Islas Viales)

**Requirement:** Identify weakly connected components, report size of the giant component and total number of islands.

**Algorithm:** BFS/DFS treating every edge as undirected.
- Maintain `visited` array.
- For each unvisited node, run BFS/DFS using both outgoing and (conceptually) incoming edges.
- Record component ID and size per component.

**Why:** BFS/DFS is the standard exact method for component decomposition. It runs in linear time and naturally gives us the giant component membership we need for downstream algorithms.

**Complexity:** O(|V| + |E|).

**Output:**
- `component_id` vector (node → component index, -1 for isolated nodes without any edges).
- `sizes` vector (component index → number of nodes).
- `giant_component_idx` (index of largest component).

---

### 2.3 Road Diameter

**Requirement:** Find the pair of intersections with the **maximum shortest-path distance** within the giant component, in meters.

**Algorithm:** Multi-sweep heuristic.
1. Pick a random node in the giant component.
2. Run Dijkstra → find farthest node `u`.
3. Run Dijkstra from `u` → find farthest node `v`.
4. Report `dist(u, v)` as the diameter estimate.

**Why exact all-pairs is infeasible:** On ~886k nodes, exact computation would require running Dijkstra from every node in the giant component: O(|V| × (|E| log |V|)), which is prohibitive on consumer hardware.

**Why multi-sweep:** Real road networks have low treewidth and are close to trees. For trees, the two-sweep method is provably exact. For road networks, it is empirically exact in >99% of cases (Handbook of Graph Drawing, "graph diameter heuristics"). It reduces the problem to just **two** Dijkstra runs while maintaining high confidence in the result. We will document it explicitly as a heuristic in the report.

**Alternatives considered:**
- Random sampling: only gives a lower bound; no quality guarantee.
- Eccentricity pruning: much more complex to implement, marginal gain.

**Complexity:** O(2 × |E| log |V|).

---

### 2.4 Minimum Emergency Network (MST)

**Requirement:** Build an MST on the giant component and report total covered distance in km.

**Algorithm:** Kruskal on deduplicated undirected edges.
- Collect every unique undirected edge `(min(u,v), max(u,v))` with its distance weight.
- Remove parallel edges (keep shortest weight).
- Sort by weight.
- Union-Find to add edges without forming cycles.
- Sum weights of accepted edges.

**Why Kruskal:** The graph is already available as an edge list (via adjacency). Kruskal only requires sorting edges and a union-find structure, both straightforward. Prim’s would need an adjacency list, which we have, but Kruskal is simpler to implement correctly with deduplication.

**Complexity:** O(|E| log |E|) for sorting + α(|V|) for union-find.

**Output:** Total MST weight in kilometers (`weight / 1000.0`).

---

### 2.5 Bonus: Distance vs. Time Routing

**Requirement:** Compare shortest path by distance vs. shortest path by time between a given source and destination.

**Algorithm:** Two independent Dijkstra runs.
1. Dijkstra minimizing `weight` (distance).
2. Dijkstra minimizing `time_weight` (time).

For each run:
- Track predecessors to reconstruct the path.
- Report total distance, total time, and number of edges.
- Also report the overlap (nodes/edges common to both paths).

**Why Dijkstra twice:** The graph is static. Running Dijkstra twice with different edge costs is the cleanest way to compare the two objectives. There is no need for a bi-criteria algorithm since we only need the two individual optima.

**Complexity:** O(2 × |E| log |V|).

---

## 3. Graph API Additions

Add the following public methods to `Graph`:

```cpp
struct ComponentInfo {
    std::vector<int> component_id;   // -1 for isolated nodes
    std::vector<size_t> sizes;       // size of each component
    size_t giant_component_idx = 0;  // index of largest component
};

// Returns Dijkstra distances from source. max_distance = -1 means no cutoff.
std::vector<double> dijkstra(uint32_t source,
                             double max_distance = -1.0,
                             bool use_time = false) const;

// Count nodes reachable within max_dist_m street distance from source.
size_t vehicleReach(uint32_t source, double max_dist_m = 5000.0) const;

// Find weakly connected components.
ComponentInfo weaklyConnectedComponents() const;

// Estimate road diameter on the giant component (multi-sweep heuristic).
double roadDiameter() const;

// Build MST on the giant component; return total weight in km.
double minimumSpanningTree() const;

// Compare distance-optimal vs. time-optimal routes.
void compareRoutes(uint32_t src, uint32_t dst) const;
```

---

## 4. Main / Menu

After data loading and graph construction, display an interactive text menu:

```
=== Bolivia Road Network Analysis ===
Loaded: <N> nodes, <E> directed edges

1. Vehicle Reach              — count nodes within 5 km of a source
2. Connected Components       — giant component size & island count
3. Road Diameter              — estimated farthest pair (multi-sweep)
4. Emergency MST              — minimum spanning tree distance (km)
5. Distance vs. Time Route    — compare two shortest paths
6. Exit

Choice:
```

Each option prompts for any required node IDs, runs the algorithm, prints formatted results, and returns to the menu.

---

## 5. Testing Strategy

- **Small subgraph:** Before running on the full 886k graph, validate on a small city subgraph (e.g., a few thousand nodes) where exact all-pairs can be computed for comparison.
- **Assert invariants:**
  - MST weight < sum of all edge weights.
  - Giant component is the largest component.
  - Vehicle reach count ≥ 1 (the source itself).
- **Timing:** Print execution time for each algorithm to satisfy the assignment’s requirement of measuring real-world performance.

---

## 6. Files to Modify

- `include/graph.h` — add new structs and method declarations.
- `src/graph.cpp` — implement all algorithms.
- `src/main.cpp` — add menu loop and wire algorithms.

No changes needed to `csv_parser.{h,cpp}` or `CMakeLists.txt`.
