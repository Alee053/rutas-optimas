#include "graph.h"
#include "csv_parser.h"

#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

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

    // Deduplicate each node's adjacency list in-place to remove parallel edges (e.g. from reverse edges)
    for (uint32_t u = 0; u <= max_id; ++u) {
        auto& edges_list = g.adj_[u];
        if (edges_list.size() <= 1) continue;

        // Sort by destination node, then by distance_m (weight)
        std::sort(edges_list.begin(), edges_list.end(), [](const Edge& a, const Edge& b) {
            if (a.to != b.to) return a.to < b.to;
            return a.weight < b.weight;
        });

        // Unique in-place: keep only the first (shortest) edge to each unique destination
        auto write_it = edges_list.begin();
        for (auto read_it = edges_list.begin() + 1; read_it != edges_list.end(); ++read_it) {
            if (read_it->to != write_it->to) {
                ++write_it;
                *write_it = *read_it;
            }
        }
        edges_list.erase(write_it + 1, edges_list.end());
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

std::vector<double> Graph::dijkstra(uint32_t source,
                                    double max_distance,
                                    bool use_time,
                                    std::vector<uint32_t>* predecessor) const {
    const double INF = std::numeric_limits<double>::infinity();
    size_t n = adj_.size();
    std::vector<double> dist(n, INF);
    if (source >= n) {
        return dist;
    }

    if (predecessor) {
        predecessor->assign(n, std::numeric_limits<uint32_t>::max());
        (*predecessor)[source] = source;
    }

    dist[source] = 0.0;
    using P = std::pair<double, uint32_t>;
    std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
    pq.emplace(0.0, source);

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d > dist[u]) continue;
        if (max_distance >= 0.0 && d > max_distance) continue;

        for (const auto& e : adj_[u]) {
            double w = use_time ? e.time_weight : e.weight;
            if (dist[e.to] > d + w) {
                dist[e.to] = d + w;
                if (predecessor) {
                    (*predecessor)[e.to] = u;
                }
                pq.emplace(dist[e.to], e.to);
            }
        }
    }
    return dist;
}

size_t Graph::vehicleReach(uint32_t source, double max_dist_m) const {
    auto dist = dijkstra(source, max_dist_m, false, nullptr);
    size_t count = 0;
    for (double d : dist) {
        if (d <= max_dist_m) {
            ++count;
        }
    }
    return count;
}

ComponentInfo Graph::weaklyConnectedComponents() const {
    ComponentInfo info;
    size_t n = adj_.size();
    info.component_id.assign(n, -1);
    std::vector<char> visited(n, 0);

    std::vector<std::vector<uint32_t>> rev(n);
    for (uint32_t u = 0; u < n; ++u) {
        for (const auto& e : adj_[u]) {
            rev[e.to].push_back(u);
        }
    }

    for (uint32_t start = 0; start < n; ++start) {
        if (visited[start]) continue;

        std::vector<uint32_t> stack;
        stack.push_back(start);
        visited[start] = 1;
        size_t comp_size = 0;
        int comp_idx = static_cast<int>(info.sizes.size());

        while (!stack.empty()) {
            uint32_t u = stack.back();
            stack.pop_back();
            info.component_id[u] = comp_idx;
            ++comp_size;

            for (const auto& e : adj_[u]) {
                if (!visited[e.to]) {
                    visited[e.to] = 1;
                    stack.push_back(e.to);
                }
            }
            for (uint32_t v : rev[u]) {
                if (!visited[v]) {
                    visited[v] = 1;
                    stack.push_back(v);
                }
            }
        }

        info.sizes.push_back(comp_size);
        if (comp_size > info.sizes[info.giant_component_idx]) {
            info.giant_component_idx = info.sizes.size() - 1;
        }
    }

    return info;
}

double Graph::roadDiameter() const {
    auto comp = weaklyConnectedComponents();
    size_t n = adj_.size();

    uint32_t start = 0;
    bool found = false;
    int giant_idx = static_cast<int>(comp.giant_component_idx);
    for (uint32_t u = 0; u < n; ++u) {
        if (comp.component_id[u] == giant_idx) {
            start = u;
            found = true;
            break;
        }
    }
    if (!found) return 0.0;

    auto dist1 = dijkstra(start, -1.0, false, nullptr);
    uint32_t u = start;
    double max_dist = 0.0;
    for (uint32_t v = 0; v < n; ++v) {
        if (comp.component_id[v] == giant_idx && std::isfinite(dist1[v])) {
            if (dist1[v] > max_dist) {
                max_dist = dist1[v];
                u = v;
            }
        }
    }

    auto dist2 = dijkstra(u, -1.0, false, nullptr);
    double diameter = 0.0;
    for (uint32_t v = 0; v < n; ++v) {
        if (comp.component_id[v] == giant_idx && std::isfinite(dist2[v])) {
            if (dist2[v] > diameter) {
                diameter = dist2[v];
            }
        }
    }

    return diameter;
}
