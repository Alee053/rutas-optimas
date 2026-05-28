#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <limits>
#include <queue>

// Forward declarations from csv_parser.h
struct ParsedNode;
struct ParsedEdge;

struct Edge {
    int      to;          // destination node id
    double   weight;      // distance in meters
    double   time_weight; // time estimate in seconds
    int      maxspeed;    // 0 = unknown
    bool     oneway;      // original oneway flag
};

struct ComponentInfo {
    std::vector<int> component_id;
    std::vector<long long> sizes;
    long long giant_component_idx = 0;
};

struct DiameterResult {
    double diameter;
    int from_node;
    int to_node;
};

class Graph {
public:
    static Graph build(const std::vector<ParsedNode>& nodes,
                       const std::vector<ParsedEdge>& edges);

    const std::vector<std::vector<Edge>>& adjacency() const { return adj_; }
    long long nodeCount() const { return static_cast<long long>(adj_.size()); }
    long long edgeCount() const;
    bool hasNode(int id) const { return id >= 0 && static_cast<size_t>(id) < adj_.size(); }

    // Dijkstra from source. max_distance < 0 means no cutoff.
    // use_time = true minimizes time_weight, false minimizes weight.
    // If predecessor != nullptr, fills it with the previous node on the shortest path.
    std::vector<double> dijkstra(int source,
                                 double max_distance = -1.0,
                                 bool use_time = false,
                                 std::vector<int>* predecessor = nullptr) const;

    // Count nodes reachable within max_dist_m street distance from source.
    long long vehicleReach(int source, double max_dist_m = 5000.0) const;

    ComponentInfo weaklyConnectedComponents() const;

    DiameterResult roadDiameter() const;

    // Build MST on the giant component. Returns total weight in kilometers.
    double minimumSpanningTree() const;

    // Compare shortest path by distance vs. by time between src and dst.
    void compareRoutes(int src, int dst) const;

private:
    std::vector<std::vector<Edge>> adj_;
};
