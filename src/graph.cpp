#include "graph.h"
#include "csv_parser.h"

#include <stdexcept>
#include <algorithm>

static double inferSpeed(const std::string& fclass) {
    if (fclass == "motorway")      return 100.0;
    if (fclass == "trunk")         return 80.0;
    if (fclass == "primary")       return 60.0;
    if (fclass == "secondary")     return 50.0;
    if (fclass == "tertiary")      return 40.0;
    if (fclass == "residential")   return 30.0;
    if (fclass == "service")       return 20.0;
    if (fclass == "unclassified")  return 20.0;
    if (fclass == "path")          return 10.0;
    if (fclass == "track")         return 10.0;
    return 20.0; // default
}

static double computeTime(double distance_m, uint32_t maxspeed, const std::string& fclass) {
    double speed_kmh = (maxspeed > 0) ? static_cast<double>(maxspeed) : inferSpeed(fclass);
    if (speed_kmh <= 0.0) speed_kmh = 1.0;
    // time = distance / speed
    // distance in meters, speed in km/h → convert speed to m/s
    double speed_ms = speed_kmh * 1000.0 / 3600.0;
    return distance_m / speed_ms;
}

Graph Graph::build(const std::vector<ParsedNode>& nodes,
                   const std::vector<ParsedEdge>& edges) {
    // Find max node id to size adjacency list
    uint32_t max_id = 0;
    for (const auto& n : nodes) {
        if (n.id > max_id) max_id = n.id;
    }

    Graph g;
    g.adj_.resize(max_id + 1);

    for (const auto& e : edges) {
        if (e.from_id > max_id || e.to_id > max_id) {
            throw std::runtime_error("Edge references invalid node id");
        }

        double time_weight = computeTime(e.distance_m, e.maxspeed, e.fclass);

        // Forward edge
        g.adj_[e.from_id].push_back({
            e.to_id,
            e.distance_m,
            time_weight,
            static_cast<uint16_t>(e.maxspeed),
            e.oneway
        });

        // Reverse edge for non-oneway streets
        if (!e.oneway) {
            g.adj_[e.to_id].push_back({
                e.from_id,
                e.distance_m,
                time_weight,
                static_cast<uint16_t>(e.maxspeed),
                e.oneway
            });
        }
    }

    return g;
}

size_t Graph::edgeCount() const {
    size_t count = 0;
    for (const auto& adj_list : adj_) {
        count += adj_list.size();
    }
    return count;
}