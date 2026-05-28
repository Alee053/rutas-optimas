#include "csv_parser.h"

#include <charconv>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <algorithm>

static std::string readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string buffer(size, '\0');
    if (!file.read(buffer.data(), size)) {
        throw std::runtime_error("Failed to read file: " + filepath);
    }
    return buffer;
}

static inline void skipHeader(const char*& p, const char* end) {
    while (p < end && *p != '\n') ++p;
    if (p < end) ++p; // skip '\n'
}

static inline bool parseInt(const char* begin, const char* end, int& out) {
    std::from_chars_result res = std::from_chars(begin, end, out);
    return res.ec == std::errc{} && res.ptr == end;
}

static inline bool parseLongLong(const char* begin, const char* end, long long& out) {
    std::from_chars_result res = std::from_chars(begin, end, out);
    return res.ec == std::errc{} && res.ptr == end;
}

static inline bool parseDouble(const char* begin, const char* end, double& out) {
    std::from_chars_result res = std::from_chars(begin, end, out);
    return res.ec == std::errc{} && res.ptr == end;
}

static inline bool parseOneway(const char* begin, const char* end) {
    // Trim whitespace
    while (begin < end && (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n')) ++begin;
    while (begin < end && (*(end - 1) == ' ' || *(end - 1) == '\t' || *(end - 1) == '\r' || *(end - 1) == '\n')) --end;
    if (begin == end) return false;

    size_t len = end - begin;
    if (len == 1) {
        char c = *begin;
        return (c == '1' || c == 't' || c == 'T' || c == 'y' || c == 'Y');
    }
    
    if (len == 4) {
        if ((begin[0] == 't' || begin[0] == 'T') &&
            (begin[1] == 'r' || begin[1] == 'R') &&
            (begin[2] == 'u' || begin[2] == 'U') &&
            (begin[3] == 'e' || begin[3] == 'E')) {
            return true;
        }
    }
    if (len == 3) {
        if ((begin[0] == 'y' || begin[0] == 'Y') &&
            (begin[1] == 'e' || begin[1] == 'E') &&
            (begin[2] == 's' || begin[2] == 'S')) {
            return true;
        }
    }
    return false;
}

static inline int parseMaxSpeed(const char* begin, const char* end) {
    while (begin < end && (*begin == ' ' || *begin == '\t')) ++begin;
    if (begin == end) return 0;
    
    int val = 0;
    bool has_digits = false;
    while (begin < end && *begin >= '0' && *begin <= '9') {
        val = val * 10 + (*begin - '0');
        has_digits = true;
        ++begin;
    }
    if (!has_digits) return 0;
    return val;
}

static inline bool isValidCoordinate(double lat, double lon) {
    if (!std::isfinite(lat) || !std::isfinite(lon)) return false;
    if (lat == 0.0 || lon == 0.0) return false;
    
    // Check if coordinates are in UTM Zone 19S (meters) or standard Lat/Lon (degrees)
    bool is_utm = (lat > 5000000.0 && lat < 12000000.0 && lon > 50000.0 && lon < 3000000.0);
    bool is_deg = (lat > -25.0 && lat < -5.0 && lon > -75.0 && lon < -50.0);
    return is_utm || is_deg;
}

std::vector<ParsedNode> CSVParser::loadNodes(const std::string& filepath,
                                             const CleansingOptions& options,
                                             ParsingStats& stats) {
    (void)options; // unused for node parsing currently
    
    std::string buffer = readFile(filepath);
    const char* p = buffer.data();
    const char* end = p + buffer.size();

    skipHeader(p, end);
    if (p >= end) {
        return {};
    }

    std::vector<ParsedNode> nodes;
    nodes.reserve(buffer.size() / 30); // rough estimate: ~30 bytes per row

    int id = 0;
    double lat = 0.0;
    double lon = 0.0;
    int field = 0;
    const char* fieldStart = p;
    const char* lineStart = p;
    bool line_valid = true;

    while (p <= end) {
        char c = (p < end) ? *p : '\n';
        if (c == ',' || c == '\n' || c == '\r') {
            if ((c == '\n' || c == '\r') && p == lineStart) {
                if (c == '\r') {
                    ++p;
                    if (p < end && *p == '\n') ++p;
                } else {
                    ++p;
                }
                field = 0;
                lineStart = p;
                fieldStart = p;
                line_valid = true;
                continue;
            }

            if (line_valid) {
                if (field == 0) {
                    if (!parseInt(fieldStart, p, id)) {
                        line_valid = false;
                        stats.malformed_nodes_skipped++;
                    }
                } else if (field == 1) {
                    if (!parseDouble(fieldStart, p, lat)) {
                        line_valid = false;
                        stats.malformed_nodes_skipped++;
                    }
                } else if (field == 2) {
                    if (!parseDouble(fieldStart, p, lon)) {
                        line_valid = false;
                        stats.malformed_nodes_skipped++;
                    }
                }
            }
            ++field;
            if (c == '\r') {
                ++p;
                if (p < end && *p == '\n') ++p;
                int fieldsParsed = field;
                field = 0;
                if (p > lineStart + 2) {
                    stats.total_nodes_read++;
                    if (fieldsParsed == 3 && line_valid) {
                        if (isValidCoordinate(lat, lon)) {
                            nodes.push_back({id, lat, lon});
                            stats.valid_nodes_kept++;
                        } else {
                            stats.out_of_bounds_nodes_skipped++;
                        }
                    } else if (fieldsParsed != 3 && line_valid) {
                        stats.malformed_nodes_skipped++;
                    }
                }
                lineStart = p;
                fieldStart = p;
                line_valid = true;
            } else if (c == '\n') {
                ++p;
                int fieldsParsed = field;
                field = 0;
                if (p > lineStart + 1) {
                    stats.total_nodes_read++;
                    if (fieldsParsed == 3 && line_valid) {
                        if (isValidCoordinate(lat, lon)) {
                            nodes.push_back({id, lat, lon});
                            stats.valid_nodes_kept++;
                        } else {
                            stats.out_of_bounds_nodes_skipped++;
                        }
                    } else if (fieldsParsed != 3 && line_valid) {
                        stats.malformed_nodes_skipped++;
                    }
                }
                lineStart = p;
                fieldStart = p;
                line_valid = true;
            } else {
                ++p;
                fieldStart = p;
            }
        } else {
            ++p;
        }
    }

    return nodes;
}

static void addDirectedEdge(long long osm_id, int from_id, int to_id, double distance_m,
                            const std::string& fclass, bool oneway, int maxspeed,
                            std::vector<ParsedEdge>& edges,
                            const CleansingOptions& options, ParsingStats& stats,
                            std::unordered_map<long long, long long>& edge_map) {
    if (options.deduplicate_edges) {
        long long key = (static_cast<long long>(from_id) << 32) | (static_cast<long long>(to_id) & 0xFFFFFFFFULL);
        auto it = edge_map.find(key);
        if (it != edge_map.end()) {
            long long idx = it->second;
            if (distance_m < edges[idx].distance_m) {
                edges[idx] = {osm_id, from_id, to_id, distance_m, fclass, oneway, maxspeed};
            }
            stats.duplicate_edges_filtered++;
        } else {
            edge_map[key] = static_cast<long long>(edges.size());
            edges.push_back({osm_id, from_id, to_id, distance_m, fclass, oneway, maxspeed});
            stats.valid_edges_kept++;
        }
    } else {
        edges.push_back({osm_id, from_id, to_id, distance_m, fclass, oneway, maxspeed});
        stats.valid_edges_kept++;
    }
}

static void processParsedEdge(long long osm_id, int from_id, int to_id, double distance_m,
                              const std::string& fclass, bool oneway, int maxspeed,
                              std::vector<ParsedEdge>& edges, const std::vector<bool>& valid_nodes,
                              const CleansingOptions& options, ParsingStats& stats,
                              std::unordered_map<long long, long long>& edge_map) {
    // 1. Validate node references
    if (from_id < 0 || from_id >= static_cast<int>(valid_nodes.size()) || !valid_nodes[from_id] ||
        to_id < 0 || to_id >= static_cast<int>(valid_nodes.size()) || !valid_nodes[to_id]) {
        stats.invalid_node_refs_filtered++;
        return;
    }
    
    // 2. Validate distance
    if (distance_m <= 0.0 || !std::isfinite(distance_m)) {
        stats.malformed_edges_skipped++;
        return;
    }

    // 3. Remove self loops
    if (options.remove_self_loops && from_id == to_id) {
        stats.self_loops_removed++;
        return;
    }

    // 4. Filter non-drivable paths
    if (options.filter_non_drivable) {
        if (fclass == "footway" || fclass == "path" || fclass == "steps" ||
            fclass == "pedestrian" || fclass == "cycleway" || fclass == "bridleway") {
            stats.non_drivable_edges_filtered++;
            return;
        }
    }

    // 5. Add forward edge
    addDirectedEdge(osm_id, from_id, to_id, distance_m, fclass, oneway, maxspeed, edges, options, stats, edge_map);

    // 6. Add reverse edge if bidirectional
    if (!oneway) {
        addDirectedEdge(osm_id, to_id, from_id, distance_m, fclass, oneway, maxspeed, edges, options, stats, edge_map);
    }
}

std::vector<ParsedEdge> CSVParser::loadEdges(const std::string& filepath,
                                             const std::vector<ParsedNode>& loaded_nodes,
                                             const CleansingOptions& options,
                                             ParsingStats& stats) {
    // Build quick lookup table for valid node IDs
    int max_node_id = 0;
    for (const auto& n : loaded_nodes) {
        if (n.id > max_node_id) {
            max_node_id = n.id;
        }
    }
    std::vector<bool> valid_nodes(static_cast<size_t>(max_node_id) + 1, false);
    for (const auto& n : loaded_nodes) {
        if (n.id >= 0) {
            valid_nodes[n.id] = true;
        }
    }

    std::string buffer = readFile(filepath);
    const char* p = buffer.data();
    const char* end = p + buffer.size();

    skipHeader(p, end);
    if (p >= end) {
        return {};
    }

    std::vector<ParsedEdge> edges;
    edges.reserve(buffer.size() / 50); // rough estimate: ~50 bytes per row

    std::unordered_map<long long, long long> edge_map;

    long long osm_id = 0;
    int from_id = 0;
    int to_id = 0;
    double distance_m = 0.0;
    std::string fclass;
    bool oneway = false;
    int maxspeed = 0;
    int field = 0;
    const char* fieldStart = p;
    const char* lineStart = p;
    bool line_valid = true;

    while (p <= end) {
        char c = (p < end) ? *p : '\n';
        if (c == ',' || c == '\n' || c == '\r') {
            if ((c == '\n' || c == '\r') && p == lineStart) {
                if (c == '\r') {
                    ++p;
                    if (p < end && *p == '\n') ++p;
                } else {
                    ++p;
                }
                field = 0;
                lineStart = p;
                fieldStart = p;
                line_valid = true;
                continue;
            }

            if (line_valid) {
                if (field == 0) {
                    if (!parseLongLong(fieldStart, p, osm_id)) {
                        line_valid = false;
                        stats.malformed_edges_skipped++;
                    }
                } else if (field == 1) {
                    if (!parseInt(fieldStart, p, from_id)) {
                        line_valid = false;
                        stats.malformed_edges_skipped++;
                    }
                } else if (field == 2) {
                    if (!parseInt(fieldStart, p, to_id)) {
                        line_valid = false;
                        stats.malformed_edges_skipped++;
                    }
                } else if (field == 3) {
                    if (!parseDouble(fieldStart, p, distance_m)) {
                        line_valid = false;
                        stats.malformed_edges_skipped++;
                    }
                } else if (field == 4) {
                    fclass.assign(fieldStart, p - fieldStart);
                } else if (field == 5) {
                    oneway = parseOneway(fieldStart, p);
                } else if (field == 6) {
                    maxspeed = parseMaxSpeed(fieldStart, p);
                }
            }
            ++field;
            if (c == '\r') {
                ++p;
                if (p < end && *p == '\n') ++p;
                int fieldsParsed = field;
                field = 0;
                if (p > lineStart + 2) {
                    stats.total_edges_read++;
                    if (fieldsParsed == 7 && line_valid) {
                        processParsedEdge(osm_id, from_id, to_id, distance_m, fclass, oneway, maxspeed,
                                          edges, valid_nodes, options, stats, edge_map);
                    } else if (fieldsParsed != 7 && line_valid) {
                        stats.malformed_edges_skipped++;
                    }
                }
                lineStart = p;
                fieldStart = p;
                line_valid = true;
            } else if (c == '\n') {
                ++p;
                int fieldsParsed = field;
                field = 0;
                if (p > lineStart + 1) {
                    stats.total_edges_read++;
                    if (fieldsParsed == 7 && line_valid) {
                        processParsedEdge(osm_id, from_id, to_id, distance_m, fclass, oneway, maxspeed,
                                          edges, valid_nodes, options, stats, edge_map);
                    } else if (fieldsParsed != 7 && line_valid) {
                        stats.malformed_edges_skipped++;
                    }
                }
                lineStart = p;
                fieldStart = p;
                line_valid = true;
            } else {
                ++p;
                fieldStart = p;
            }
        } else {
            ++p;
        }
    }

    return edges;
}
