#include "csv_parser.h"

#include <charconv>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

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

static inline bool parseUint32(const char* begin, const char* end, uint32_t& out) {
    std::from_chars_result res = std::from_chars(begin, end, out);
    return res.ec == std::errc{} && res.ptr == end;
}

static inline bool parseUint64(const char* begin, const char* end, uint64_t& out) {
    std::from_chars_result res = std::from_chars(begin, end, out);
    return res.ec == std::errc{} && res.ptr == end;
}

static inline bool parseDouble(const char* begin, const char* end, double& out) {
    std::from_chars_result res = std::from_chars(begin, end, out);
    return res.ec == std::errc{} && res.ptr == end;
}

static inline bool parseBool(const char* begin, const char* end, bool& out) {
    if (begin + 1 == end && *begin == '0') {
        out = false;
        return true;
    }
    if (begin + 1 == end && *begin == '1') {
        out = true;
        return true;
    }
    return false;
}

std::vector<ParsedNode> CSVParser::loadNodes(const std::string& filepath) {
    std::string buffer = readFile(filepath);
    const char* p = buffer.data();
    const char* end = p + buffer.size();

    skipHeader(p, end);

    std::vector<ParsedNode> nodes;
    nodes.reserve(buffer.size() / 30); // rough estimate: ~30 bytes per row

    uint32_t id = 0;
    double lat = 0.0;
    double lon = 0.0;
    int field = 0;
    const char* fieldStart = p;

    while (p <= end) {
        char c = (p < end) ? *p : '\n';
        if (c == ',' || c == '\n' || c == '\r') {
            if (field == 0) {
                if (!parseUint32(fieldStart, p, id)) {
                    throw std::runtime_error("Invalid node_id");
                }
            } else if (field == 1) {
                if (!parseDouble(fieldStart, p, lat)) {
                    throw std::runtime_error("Invalid lat");
                }
            } else if (field == 2) {
                if (!parseDouble(fieldStart, p, lon)) {
                    throw std::runtime_error("Invalid lon");
                }
            }
            ++field;
            if (c == '\r') {
                ++p;
                if (p < end && *p == '\n') ++p;
                field = 0;
                if (p > fieldStart + 2) { // not empty line
                    nodes.push_back({id, lat, lon});
                }
                fieldStart = p;
            } else if (c == '\n') {
                ++p;
                field = 0;
                if (p > fieldStart + 2) { // not empty line
                    nodes.push_back({id, lat, lon});
                }
                fieldStart = p;
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

std::vector<ParsedEdge> CSVParser::loadEdges(const std::string& filepath) {
    std::string buffer = readFile(filepath);
    const char* p = buffer.data();
    const char* end = p + buffer.size();

    skipHeader(p, end);

    std::vector<ParsedEdge> edges;
    edges.reserve(buffer.size() / 50); // rough estimate: ~50 bytes per row

    uint64_t osm_id = 0;
    uint32_t from_id = 0;
    uint32_t to_id = 0;
    double distance_m = 0.0;
    std::string fclass;
    bool oneway = false;
    uint32_t maxspeed = 0;
    int field = 0;
    const char* fieldStart = p;

    while (p <= end) {
        char c = (p < end) ? *p : '\n';
        if (c == ',' || c == '\n' || c == '\r') {
            if (field == 0) {
                if (!parseUint64(fieldStart, p, osm_id)) {
                    throw std::runtime_error("Invalid osm_id");
                }
            } else if (field == 1) {
                if (!parseUint32(fieldStart, p, from_id)) {
                    throw std::runtime_error("Invalid from_id");
                }
            } else if (field == 2) {
                if (!parseUint32(fieldStart, p, to_id)) {
                    throw std::runtime_error("Invalid to_id");
                }
            } else if (field == 3) {
                if (!parseDouble(fieldStart, p, distance_m)) {
                    throw std::runtime_error("Invalid distance_m");
                }
            } else if (field == 4) {
                fclass.assign(fieldStart, p - fieldStart);
            } else if (field == 5) {
                if (!parseBool(fieldStart, p, oneway)) {
                    throw std::runtime_error("Invalid oneway");
                }
            } else if (field == 6) {
                if (p == fieldStart) {
                    maxspeed = 0;
                } else if (!parseUint32(fieldStart, p, maxspeed)) {
                    throw std::runtime_error("Invalid maxspeed");
                }
            }
            ++field;
            if (c == '\r') {
                ++p;
                if (p < end && *p == '\n') ++p;
                field = 0;
                if (p > fieldStart + 2) {
                    edges.push_back({osm_id, from_id, to_id, distance_m, fclass, oneway, maxspeed});
                }
                fieldStart = p;
            } else if (c == '\n') {
                ++p;
                field = 0;
                if (p > fieldStart + 2) {
                    edges.push_back({osm_id, from_id, to_id, distance_m, fclass, oneway, maxspeed});
                }
                fieldStart = p;
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
