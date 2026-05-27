#include <iostream>
#include <iomanip>
#include "csv_parser.h"
#include "graph.h"

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

        if (graph.nodeCount() > 0) {
            // Find a node that actually has outgoing edges to display as a test
            size_t sample_node = 0;
            for (size_t i = 0; i < graph.nodeCount(); ++i) {
                if (!graph.adjacency()[i].empty()) {
                    sample_node = i;
                    break;
                }
            }
            const auto& adj_list = graph.adjacency()[sample_node];
            std::cout << "Node " << sample_node << " has " << adj_list.size()
                      << " outgoing edges." << std::endl;
            if (!adj_list.empty()) {
                const auto& e = adj_list[0];
                std::cout << "First edge from node " << sample_node << ": to=" << e.to
                          << " dist=" << e.weight << "m time=" << e.time_weight
                          << "s maxspeed=" << e.maxspeed << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
