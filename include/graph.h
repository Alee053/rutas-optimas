#pragma once

#include <cstdint>
#include <vector>
#include <string>

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

class Graph {
public:
    static Graph build(const std::vector<ParsedNode>& nodes,
                       const std::vector<ParsedEdge>& edges);

    const std::vector<std::vector<Edge>>& adjacency() const { return adj_; }
    size_t nodeCount() const { return adj_.size(); }
    size_t edgeCount() const;
    bool hasNode(uint32_t id) const { return id < adj_.size(); }

private:
    std::vector<std::vector<Edge>> adj_;
};
