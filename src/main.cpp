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
                    int src;
                    if (std::cin >> src && graph.hasNode(src)) {
                        auto t0 = std::chrono::steady_clock::now();
                        long long reach = graph.vehicleReach(src);
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
                    
                    long long total_nodes = graph.nodeCount();
                    long long total_edges = graph.edgeCount();
                    double total_avg_degree = total_nodes > 0 ? static_cast<double>(total_edges) / total_nodes : 0.0;

                    long long giant_idx = comp.giant_component_idx;
                    long long giant_nodes = comp.sizes[giant_idx];
                    long long giant_edges = 0;
                    for (int u = 0; u < graph.nodeCount(); ++u) {
                        if (comp.component_id[static_cast<size_t>(u)] == giant_idx) {
                            for (const auto& e : graph.adjacency()[static_cast<size_t>(u)]) {
                                if (comp.component_id[static_cast<size_t>(e.to)] == giant_idx) {
                                    giant_edges++;
                                }
                            }
                        }
                    }
                    double giant_avg_degree = giant_nodes > 0 ? static_cast<double>(giant_edges) / giant_nodes : 0.0;
                    
                    auto t1 = std::chrono::steady_clock::now();
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
                    
                    std::cout << "Number of islands (components): " << comp.sizes.size() << "\n";
                    std::cout << "Giant component size: " << giant_nodes << " nodes\n";
                    std::cout << "----------------------------------------------------------\n";
                    std::cout << "METRIC COMPARISON (Giant Component vs. Complete Filtered):\n";
                    std::cout << "  Nodos:          " << giant_nodes << " vs. " << total_nodes << "\n";
                    std::cout << "  Aristas:        " << giant_edges << " vs. " << total_edges << "\n";
                    std::cout << "  Grado promedio: " << std::fixed << std::setprecision(4) << giant_avg_degree << " vs. " << total_avg_degree << "\n";
                    std::cout << "----------------------------------------------------------\n";
                    std::cout << "Time: " << ms << " ms\n";
                    break;
                }
                case 3: {
                    auto t0 = std::chrono::steady_clock::now();
                    auto res = graph.roadDiameter();
                    auto t1 = std::chrono::steady_clock::now();
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
                    std::cout << "Estimated road diameter: " << std::fixed << std::setprecision(2)
                              << res.diameter << " m\n";
                    std::cout << "  From Node ID: " << res.from_node << "\n";
                    std::cout << "  To Node ID:   " << res.to_node << "\n";
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
                    int src;
                    if (!(std::cin >> src) || !graph.hasNode(src)) {
                        std::cout << "Invalid source node ID.\n";
                        std::cin.clear();
                        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                        break;
                    }
                    std::cout << "Enter destination node ID: ";
                    int dst;
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