#include <iostream>
#include "graph.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <as-rel2-file>" << std::endl;
        return 1;
    }

    Graph graph = loadGraph(argv[1]);

    if (hasCycle(graph)) {
        std::cerr << "Error: provider/customer cycle detected in the graph." << std::endl;
        return 1;
    }

    std::cout << "Loaded " << graph.ases.size() << " ASes." << std::endl;

    int providerEdges = 0;
    int peerEdges = 0;
    for (auto it = graph.ases.begin(); it != graph.ases.end(); it++) {
        providerEdges += it->second.customers.size();
        peerEdges += it->second.peers.size();
    }

    std::cout << "Provider->customer relationships: " << providerEdges << std::endl;
    std::cout << "Peer-to-peer relationships: " << peerEdges / 2 << std::endl;

    return 0;
}
