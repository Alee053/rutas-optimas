# Design: In-Memory Graph Representation for Road Network

**Date:** 2026-05-24  
**Project:** Rutas Óptimas en Red Vial Urbana  
**Scope:** Graph memory layout and adjacency structure — isolated from CSV parsing and algorithm logic.

---

## 1. Context

After parsing `nodes.csv` (~886,790 rows) and `edges.csv` (~588,485 rows), we need an in-memory graph representation that serves all five algorithmic objectives:

1. **Reachability within 5km** (Dijkstra/BFS with distance threshold)
2. **Weakly connected components** (BFS/DFS treating edges as undirected)
3. **Graph diameter** (longest shortest path in giant component)
4. **Minimum Spanning Tree** (Kruskal or Prim on giant component)
5. **Bonus: Distance vs time shortest path** (Dijkstra with two weight functions)

Key observations:
- **Sparse graph**: ~588K edges / ~886K nodes ≈ 0.66 avg degree
- **Directed edges**: `oneway` field indicates one-way streets
- **Small enough for RAM**: Even with reverse edges, total memory stays well under 100 MB
- **Speed priority**: Extra memory (100 MB) is acceptable for faster traversals

---

## 2. Goals

- **Single generic structure**: One adjacency list serves both directed and undirected algorithms
- **Fast traversals**: Adjacency list with cache-friendly contiguous storage
- **Zero code duplication**: BFS, Dijkstra, Kruskal all consume the same graph
- **Edge metadata preserved**: `distance_m`, `fclass`, `oneway`, `maxspeed` available during traversals
- **Simple to explain**: Straightforward enough for a university report

---

## 3. Data Structures

### 3.1 Edge Structure

```cpp
struct Edge {
    uint32_t to;        // destination node id
    double   weight;    // distance_m (primary weight)
    double   time_weight; // time estimate in seconds (derived from maxspeed or fclass)
    uint16_t maxspeed;  // 0 = unknown
    bool     oneway;    // original oneway flag from CSV
};
```

**Why this layout?**
- `to` first → most frequently accessed during BFS/Dijkstra (cache-friendly)
- `weight` second → primary Dijkstra weight
- `time_weight` → precomputed for the bonus objective (avoids re-computation during Dijkstra)
- `maxspeed` and `oneway` → preserve original metadata for analysis/reporting

### 3.2 Graph Structure

```cpp
class Graph {
public:
    // Build graph from parsed CSV data
    static Graph build(const std::vector<ParsedNode>& nodes,
                       const std::vector<ParsedEdge>& edges);

    // Adjacency list: adj[node_id] = vector of outgoing edges
    const std::vector<std::vector<Edge>>& adjacency() const { return adj_; }

    // Node count
    size_t nodeCount() const { return adj_.size(); }

    // Edge count (directed edges, including auto-reversed)
    size_t edgeCount() const;

    // Check if node exists (id within bounds)
    bool hasNode(uint32_t id) const { return id < adj_.size(); }

private:
    std::vector<std::vector<Edge>> adj_;
};
```

---

## 4. Architecture

### 4.1 Build Strategy (Approach A: Always-Directed with Auto-Reverse)

```
For each ParsedEdge e in edges:
    Add directed Edge to adj_[e.from_id]:
        to = e.to_id
        weight = e.distance_m
        time_weight = computeTime(e.distance_m, e.maxspeed, e.fclass)
        maxspeed = e.maxspeed
        oneway = e.oneway
    
    If e.oneway == false:
        Add reverse Edge to adj_[e.to_id]:
            to = e.from_id
            weight = e.distance_m (same distance back)
            time_weight = computeTime(e.distance_m, e.maxspeed, e.fclass)
            maxspeed = e.maxspeed
            oneway = e.oneway
```

**Key insight**: By always inserting reverse edges for non-oneway streets, the graph becomes **implicitly undirected** for BFS/component analysis, while still respecting directionality for oneway streets.

### 4.2 Time Weight Computation

```cpp
double computeTime(double distance_m, uint32_t maxspeed, const std::string& fclass) {
    // If maxspeed is explicit (> 0), use it
    // Otherwise, infer from fclass:
    //   motorway → 100 km/h, trunk → 80, primary → 60, secondary → 50,
    //   tertiary → 40, residential → 30, service/unclassified → 20
    // Time = distance / speed → convert to seconds
}
```

### 4.3 Memory Layout

| Component | Estimate |
|-----------|----------|
| Raw edges (~588K) | ~588K × 40 bytes ≈ 23 MB |
| Reverse edges (~50% of edges) | ~294K × 40 bytes ≈ 12 MB |
| Adjacency vectors overhead | ~886K × 24 bytes (vector struct) ≈ 21 MB |
| **Total** | **~56 MB** |

This fits comfortably within the 100 MB budget and leaves plenty of headroom.

---

## 5. Algorithm Usage Patterns

### 5.1 Directed Algorithms (Dijkstra shortest path)

```cpp
// Use adjacency list as-is — respects oneway streets
for (const Edge& e : graph.adjacency()[u]) {
    relax(u, e.to, e.weight);  // or e.time_weight for time-based
}
```

### 5.2 Undirected Algorithms (Components, MST, Diameter)

```cpp
// Same adjacency list — reverse edges make it undirected
// Components: BFS/DFS traverses all outgoing edges (including reverse)
// MST: Kruskal collects all edges, undirected by construction
// Diameter: BFS from each node in component, same as undirected
```

**Why this works:**
- Components: BFS follows all outgoing edges. Reverse edges ensure both directions are reachable, so weak connectivity is detected naturally.
- MST: Kruskal sorts all edges and treats them as undirected (reverse edges are just parallel edges with same weight, which Kruskal handles correctly).
- Diameter: BFS shortest paths use all outgoing edges; reverse edges provide the undirected paths.

---

## 6. Edge Cases

| Scenario | Handling |
|----------|----------|
| Disconnected node (no edges) | Empty adjacency vector, BFS won't reach it |
| Node id gaps | `Graph::build` resizes `adj_` to `max(node_id) + 1`, unused slots remain empty |
| Oneway only (no reverse) | No reverse edge added; undirected algorithms treat it as directed-only |
| Missing maxspeed | `time_weight` falls back to fclass-based inference, or 0 if unknown |
| Self-loop (from == to) | Added as single edge (no reverse needed since it points to itself) |

---

## 7. Error Handling

- **Invalid node ids in edges**: Throw `std::runtime_error` during `Graph::build` if an edge references a node id >= node count
- **Empty graph**: `Graph::build` returns empty graph; algorithms should handle this gracefully

---

## 8. Testing Strategy

- **Build test**: Parse sample CSVs, build graph, verify node count and edge count
- **Adjacency test**: Verify reverse edges exist for non-oneway streets
- **Oneway test**: Verify no reverse edge exists for oneway streets
- **Disconnected node test**: Verify isolated nodes have empty adjacency

---

## 9. File Layout

```
include/
  graph.h         // Graph class + Edge struct declaration
src/
  graph.cpp       // Graph::build + time weight computation
src/
  main.cpp        // Calls CSVParser, then Graph::build, then algorithms
```

---

## 10. Success Criteria

- [ ] Graph builds successfully from parsed CSV data
- [ ] Memory footprint stays under 100 MB
- [ ] All five objectives can be implemented using the same adjacency structure
- [ ] No code duplication between directed and undirected algorithm paths
- [ ] Edge metadata (distance, time, fclass, maxspeed) accessible during traversals
