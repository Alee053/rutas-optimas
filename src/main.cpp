#include <iostream>
#include "csv_parser.h"
#include "graph.h"

int main() {
    try {
        auto nodes = CSVParser::loadNodes("../data/nodes.csv");
        auto edges = CSVParser::loadEdges("../data/edges.csv");

        std::cout << "Loaded " << nodes.size() << " nodes and "
                  << edges.size() << " edges from CSV." << std::endl;

        auto graph = Graph::build(nodes, edges);
        std::cout << "Graph built: " << graph.nodeCount() << " nodes, "
                  << graph.edgeCount() << " directed edges." << std::endl;

        if (!graph.adjacency().empty()) {
            const auto& first_adj = graph.adjacency()[0];
            std::cout << "Node 0 has " << first_adj.size()
                      << " outgoing edges." << std::endl;
            if (!first_adj.empty()) {
                const auto& e = first_adj[0];
                std::cout << "First edge from node 0: to=" << e.to
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
