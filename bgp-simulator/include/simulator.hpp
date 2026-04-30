#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include <vector>
#include "graph.hpp"

void propagateAnnouncements(Graph& graph, const std::vector<std::vector<int> >& ranks);

#endif