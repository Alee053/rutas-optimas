#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ParsedNode {
    uint32_t id;
    double   lat;
    double   lon;
};

struct ParsedEdge {
    uint64_t    osm_id;
    uint32_t    from_id;
    uint32_t    to_id;
    double      distance_m;
    std::string fclass;
    bool        oneway;
    uint32_t    maxspeed;
};

struct CleansingOptions {
    bool filter_non_drivable = true;
    bool remove_self_loops   = true;
    bool deduplicate_edges   = true;
};

struct ParsingStats {
    size_t total_nodes_read = 0;
    size_t valid_nodes_kept = 0;
    size_t malformed_nodes_skipped = 0;
    size_t out_of_bounds_nodes_skipped = 0;

    size_t total_edges_read = 0;
    size_t valid_edges_kept = 0;
    size_t malformed_edges_skipped = 0;
    size_t self_loops_removed = 0;
    size_t duplicate_edges_filtered = 0;
    size_t non_drivable_edges_filtered = 0;
    size_t invalid_node_refs_filtered = 0;
};

class CSVParser {
public:
    static std::vector<ParsedNode> loadNodes(const std::string& filepath,
                                             const CleansingOptions& options,
                                             ParsingStats& stats);
                                             
    static std::vector<ParsedEdge> loadEdges(const std::string& filepath,
                                             const std::vector<ParsedNode>& loaded_nodes,
                                             const CleansingOptions& options,
                                             ParsingStats& stats);
};

