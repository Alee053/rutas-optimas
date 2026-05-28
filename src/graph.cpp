#include "graph.h"
#include "csv_parser.h"

#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_set>
#include <chrono>
#include <iostream>
#include <iomanip>

namespace {
struct UnionFind {
    std::vector<int> parent;
    explicit UnionFind(int n) : parent(static_cast<size_t>(n)) {
        for (int i = 0; i < n; ++i) parent[static_cast<size_t>(i)] = i;
    }
    int find(int x) {
        size_t ux = static_cast<size_t>(x);
        if (parent[ux] != x) parent[ux] = find(parent[ux]);
        return parent[ux];
    }
    bool unite(int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra == rb) return false;
        parent[static_cast<size_t>(ra)] = rb;
        return true;
    }
};
} // namespace

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

static double computeTime(double distance_m, int maxspeed, const std::string& fclass) {
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
    int max_id = 0;
    for (const auto& n : nodes) {
        if (n.id > max_id) max_id = n.id;
    }

    Graph g;
    g.adj_.resize(static_cast<size_t>(max_id) + 1);

    for (const auto& e : edges) {
        if (e.from_id < 0 || e.from_id > max_id || e.to_id < 0 || e.to_id > max_id) {
            throw std::runtime_error("Edge references invalid node id");
        }

        double time_weight = computeTime(e.distance_m, e.maxspeed, e.fclass);

        // Forward edge
        g.adj_[static_cast<size_t>(e.from_id)].push_back({
            e.to_id,
            e.distance_m,
            time_weight,
            e.maxspeed,
            e.oneway
        });

        // Reverse edge for non-oneway streets
        if (!e.oneway) {
            g.adj_[static_cast<size_t>(e.to_id)].push_back({
                e.from_id,
                e.distance_m,
                time_weight,
                e.maxspeed,
                e.oneway
            });
        }
    }

    // Deduplicate each node's adjacency list in-place to remove parallel edges (e.g. from reverse edges)
    for (int u = 0; u <= max_id; ++u) {
        auto& edges_list = g.adj_[static_cast<size_t>(u)];
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

long long Graph::edgeCount() const {
    long long count = 0;
    for (const auto& adj_list : adj_) {
        count += static_cast<long long>(adj_list.size());
    }
    return count;
}

std::vector<double> Graph::dijkstra(int source,
                                    double max_distance,
                                    bool use_time,
                                    std::vector<int>* predecessor) const {
    const double INF = std::numeric_limits<double>::infinity();
    size_t n = adj_.size();
    std::vector<double> dist(n, INF);
    if (source < 0 || static_cast<size_t>(source) >= n) {
        return dist;
    }

    if (predecessor) {
        predecessor->assign(n, -1);
        (*predecessor)[static_cast<size_t>(source)] = source;
    }

    dist[static_cast<size_t>(source)] = 0.0;
    using P = std::pair<double, int>;
    std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
    pq.emplace(0.0, source);

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        size_t uu = static_cast<size_t>(u);
        if (d > dist[uu]) continue;
        if (max_distance >= 0.0 && d > max_distance) break;

        for (const auto& e : adj_[uu]) {
            size_t eto = static_cast<size_t>(e.to);
            double w = use_time ? e.time_weight : e.weight;
            if (dist[eto] > d + w) {
                dist[eto] = d + w;
                if (predecessor) {
                    (*predecessor)[eto] = u;
                }
                pq.emplace(dist[eto], e.to);
            }
        }
    }
    return dist;
}

long long Graph::vehicleReach(int source, double max_dist_m) const {
    auto dist = dijkstra(source, max_dist_m, false, nullptr);
    long long count = 0;
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

    std::vector<std::vector<int>> rev(n);
    for (int u = 0; u < static_cast<int>(n); ++u) {
        for (const auto& e : adj_[static_cast<size_t>(u)]) {
            rev[static_cast<size_t>(e.to)].push_back(u);
        }
    }

    for (int start = 0; start < static_cast<int>(n); ++start) {
        if (visited[static_cast<size_t>(start)]) continue;

        std::vector<int> stack;
        stack.push_back(start);
        visited[static_cast<size_t>(start)] = 1;
        long long comp_size = 0;
        int comp_idx = static_cast<int>(info.sizes.size());

        while (!stack.empty()) {
            int u = stack.back();
            stack.pop_back();
            info.component_id[static_cast<size_t>(u)] = comp_idx;
            ++comp_size;

            for (const auto& e : adj_[static_cast<size_t>(u)]) {
                size_t eto = static_cast<size_t>(e.to);
                if (!visited[eto]) {
                    visited[eto] = 1;
                    stack.push_back(e.to);
                }
            }
            for (int v : rev[static_cast<size_t>(u)]) {
                size_t vv = static_cast<size_t>(v);
                if (!visited[vv]) {
                    visited[vv] = 1;
                    stack.push_back(v);
                }
            }
        }

        info.sizes.push_back(comp_size);
        if (comp_size > info.sizes[static_cast<size_t>(info.giant_component_idx)]) {
            info.giant_component_idx = static_cast<long long>(info.sizes.size()) - 1;
        }
    }

    return info;
}

DiameterResult Graph::roadDiameter() const {
    auto comp = weaklyConnectedComponents();
    size_t n = adj_.size();

    int start = 0;
    bool found = false;
    int giant_idx = static_cast<int>(comp.giant_component_idx);
    for (int u = 0; u < static_cast<int>(n); ++u) {
        if (comp.component_id[static_cast<size_t>(u)] == giant_idx) {
            start = u;
            found = true;
            break;
        }
    }
    if (!found) return {0.0, -1, -1};

    auto dist1 = dijkstra(start, -1.0, false, nullptr);
    int u = start;
    double max_dist = 0.0;
    for (int v = 0; v < static_cast<int>(n); ++v) {
        size_t vv = static_cast<size_t>(v);
        if (comp.component_id[vv] == giant_idx && std::isfinite(dist1[vv])) {
            if (dist1[vv] > max_dist) {
                max_dist = dist1[vv];
                u = v;
            }
        }
    }

    auto dist2 = dijkstra(u, -1.0, false, nullptr);
    double diameter = 0.0;
    int w = u;
    for (int v = 0; v < static_cast<int>(n); ++v) {
        size_t vv = static_cast<size_t>(v);
        if (comp.component_id[vv] == giant_idx && std::isfinite(dist2[vv])) {
            if (dist2[vv] > diameter) {
                diameter = dist2[vv];
                w = v;
            }
        }
    }

    return {diameter, u, w};
}

double Graph::minimumSpanningTree() const {
    auto comp = weaklyConnectedComponents();
    size_t n = adj_.size();
    int giant_idx = static_cast<int>(comp.giant_component_idx);

    struct UndirEdge {
        int u, v;
        double weight;
        bool operator<(const UndirEdge& other) const {
            return weight < other.weight;
        }
    };

    std::vector<UndirEdge> undir_edges;
    undir_edges.reserve(n * 2);

    for (int u = 0; u < static_cast<int>(n); ++u) {
        size_t uu = static_cast<size_t>(u);
        if (comp.component_id[uu] != giant_idx) continue;
        for (const auto& e : adj_[uu]) {
            if (comp.component_id[static_cast<size_t>(e.to)] != giant_idx) continue;
            int a = std::min(u, e.to);
            int b = std::max(u, e.to);
            undir_edges.push_back({a, b, e.weight});
        }
    }

    std::sort(undir_edges.begin(), undir_edges.end(), [](const UndirEdge& a, const UndirEdge& b) {
        if (a.u != b.u) return a.u < b.u;
        if (a.v != b.v) return a.v < b.v;
        return a.weight < b.weight;
    });

    std::vector<UndirEdge> unique_edges;
    unique_edges.reserve(undir_edges.size());
    for (const auto& e : undir_edges) {
        if (unique_edges.empty() ||
            unique_edges.back().u != e.u ||
            unique_edges.back().v != e.v) {
            unique_edges.push_back(e);
        }
    }

    UnionFind uf(static_cast<int>(n));
    double total = 0.0;
    long long edges_used = 0;
    long long target_edges = comp.sizes[static_cast<size_t>(giant_idx)] > 0 ? comp.sizes[static_cast<size_t>(giant_idx)] - 1 : 0;

    for (const auto& e : unique_edges) {
        if (uf.unite(e.u, e.v)) {
            total += e.weight;
            ++edges_used;
            if (edges_used == target_edges) break;
        }
    }

    return total / 1000.0;
}

void Graph::compareRoutes(int src, int dst) const {
    if (!hasNode(src) || !hasNode(dst)) {
        std::cout << "Invalid node ID(s)." << std::endl;
        return;
    }

    auto start = std::chrono::steady_clock::now();

    std::vector<int> prev_d;
    auto dist_d = dijkstra(src, -1.0, false, &prev_d);

    std::vector<int> prev_t;
    auto dist_t = dijkstra(src, -1.0, true, &prev_t);

    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    auto reconstruct = [](const std::vector<int>& prev, int s, int t) {
        std::vector<int> path;
        if (prev.empty() || prev[static_cast<size_t>(t)] == -1) {
            return path;
        }
        int cur = t;
        while (cur != s) {
            path.push_back(cur);
            cur = prev[static_cast<size_t>(cur)];
        }
        path.push_back(s);
        std::reverse(path.begin(), path.end());
        return path;
    };

    auto path_d = reconstruct(prev_d, src, dst);
    auto path_t = reconstruct(prev_t, src, dst);

    auto pathMetric = [&](const std::vector<int>& path, bool use_time) -> double {
        double total = 0.0;
        for (size_t i = 0; i + 1 < path.size(); ++i) {
            int u = path[i];
            int v = path[i + 1];
            bool found = false;
            for (const auto& e : adj_[static_cast<size_t>(u)]) {
                if (e.to == v) {
                    total += use_time ? e.time_weight : e.weight;
                    found = true;
                    break;
                }
            }
            if (!found) return std::numeric_limits<double>::infinity();
        }
        return total;
    };

    double total_time_d = pathMetric(path_d, true);
    double total_dist_t = pathMetric(path_t, false);

    std::unordered_set<int> nodes_d(path_d.begin(), path_d.end());
    size_t overlap = 0;
    for (int node : path_t) {
        if (nodes_d.count(node)) ++overlap;
    }

    std::cout << "\n--- Distance-optimal route ---\n";
    if (path_d.empty()) {
        std::cout << "No path found.\n";
    } else {
        std::cout << "Distance: " << std::fixed << std::setprecision(2) << dist_d[static_cast<size_t>(dst)] << " m\n";
        std::cout << "Time: " << std::fixed << std::setprecision(2) << total_time_d << " s\n";
        std::cout << "Edges: " << (path_d.size() - 1) << "\n";
    }

    std::cout << "\n--- Time-optimal route ---\n";
    if (path_t.empty()) {
        std::cout << "No path found.\n";
    } else {
        std::cout << "Distance: " << std::fixed << std::setprecision(2) << total_dist_t << " m\n";
        std::cout << "Time: " << std::fixed << std::setprecision(2) << dist_t[static_cast<size_t>(dst)] << " s\n";
        std::cout << "Edges: " << (path_t.size() - 1) << "\n";
    }

    std::cout << "\nOverlap (shared nodes): " << overlap << "\n";
    std::cout << "Total computation time: " << ms << " ms\n";
}
