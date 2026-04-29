#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <map>
#include <string>
#include "as.hpp"

struct Graph {
    std::map<int, AS> ases;
};

Graph loadGraph(const std::string& filepath);
bool hasCycle(const Graph& graph);

#endif
