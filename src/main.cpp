#include <iostream>
#include "csv_parser.h"

int main() {
    try {
        auto nodes = CSVParser::loadNodes("data/nodes.csv");
        auto edges = CSVParser::loadEdges("data/edges.csv");

        std::cout << "Loaded " << nodes.size() << " nodes and "
                  << edges.size() << " edges." << std::endl;

        if (!nodes.empty()) {
            const auto& first = nodes.front();
            std::cout << "First node: id=" << first.id
                      << " lat=" << first.lat << " lon=" << first.lon << std::endl;
        }

        if (!edges.empty()) {
            const auto& first = edges.front();
            std::cout << "First edge: osm_id=" << first.osm_id
                      << " from=" << first.from_id << " to=" << first.to_id
                      << " dist=" << first.distance_m << " fclass=" << first.fclass
                      << " oneway=" << first.oneway << " maxspeed=" << first.maxspeed << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
