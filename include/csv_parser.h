#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ParsedNode {
    int    id;
    double lat;
    double lon;
};

struct ParsedEdge {
    long long osm_id;
    int       from_id;
    int       to_id;
    double    distance_m;
    std::string fclass;
    bool      oneway;
    int       maxspeed;
};

struct CleansingOptions {
    bool filter_non_drivable = true;
    bool remove_self_loops   = true;
    bool deduplicate_edges   = true;
};

struct ParsingStats {
    long long total_nodes_read = 0;
    long long valid_nodes_kept = 0;
    long long malformed_nodes_skipped = 0;
    long long out_of_bounds_nodes_skipped = 0;

    long long total_edges_read = 0;
    long long valid_edges_kept = 0;
    long long malformed_edges_skipped = 0;
    long long self_loops_removed = 0;
    long long duplicate_edges_filtered = 0;
    long long non_drivable_edges_filtered = 0;
    long long invalid_node_refs_filtered = 0;
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

