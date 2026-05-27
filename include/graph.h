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
    uint32_t to;        // destination node id
    double   weight;    // distance in meters
    double   time_weight; // time estimate in seconds
    uint16_t maxspeed;  // 0 = unknown
    bool     oneway;    // original oneway flag
};

struct ComponentInfo {
    std::vector<int> component_id;
    std::vector<size_t> sizes;
    size_t giant_component_idx = 0;
};

class Graph {
public:
    static Graph build(const std::vector<ParsedNode>& nodes,
                       const std::vector<ParsedEdge>& edges);

    const std::vector<std::vector<Edge>>& adjacency() const { return adj_; }
    size_t nodeCount() const { return adj_.size(); }
    size_t edgeCount() const;
    bool hasNode(uint32_t id) const { return id < adj_.size(); }

    // Dijkstra from source. max_distance < 0 means no cutoff.
    // use_time = true minimizes time_weight, false minimizes weight.
    // If predecessor != nullptr, fills it with the previous node on the shortest path.
    std::vector<double> dijkstra(uint32_t source,
                                 double max_distance = -1.0,
                                 bool use_time = false,
                                 std::vector<uint32_t>* predecessor = nullptr) const;

    // Count nodes reachable within max_dist_m street distance from source.
    size_t vehicleReach(uint32_t source, double max_dist_m = 5000.0) const;

    ComponentInfo weaklyConnectedComponents() const;

private:
    std::vector<std::vector<Edge>> adj_;
};
