#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <cstdint>
#include <string>
#include <vector>

struct ParsedNode {
    uint32_t id;
    double   lat;
    double   lon;
};

struct ParsedEdge {
    uint64_t    osm_id;
    uint32_t    from_id;
    uint32_t    to_id;
    double      distance_m;
    std::string fclass;
    bool        oneway;
    uint32_t    maxspeed;
};

class CSVParser {
public:
    static std::vector<ParsedNode> loadNodes(const std::string& filepath);
    static std::vector<ParsedEdge> loadEdges(const std::string& filepath);
};

#endif // CSV_PARSER_H
