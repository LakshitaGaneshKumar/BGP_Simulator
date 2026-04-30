#include <iostream>
#include "../include/graph.hpp"
#include "../include/announcement.hpp"
#include "../include/bgp_policy.hpp"
#include "../include/rov_policy.hpp"
#include "../include/simulator.hpp"

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cout << "Usage: " << argv[0]
                  << " <as-rel2-file> <anns.csv> <rov_asns.csv> <output.csv>"
                  << std::endl;
        return 1;
    }

    Graph graph = loadGraph(argv[1]);

    if (hasCycle(graph)) {
        std::cerr << "Error: provider/customer cycle detected in the graph." << std::endl;
        return 1;
    }

    applyRovPolicies(graph, argv[3]);
    loadAnnouncements(graph, argv[2]);

    std::vector<std::vector<int> > ranks = buildPropagationRanks(graph);
    propagateAnnouncements(graph, ranks);

    dumpGraphToCsv(graph, argv[4]);
    std::cout << "Wrote " << argv[4] << std::endl;

    return 0;
}