#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include <string>
#include <vector>
#include "graph.hpp"

void propagateAnnouncements(Graph& graph, const std::vector<std::vector<int> >& ranks);
void dumpGraphToCsv(const Graph& graph, const std::string& outputPath);
void loadAnnouncements(Graph& graph, const std::string& filePath);

#endif