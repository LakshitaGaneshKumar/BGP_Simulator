#include <iostream>
#include "graph.hpp"
#include "../include/announcement.hpp"
#include "../include/bgp_policy.hpp"

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

    int origin_asn = 1;

    if (graph.ases.find(origin_asn) == graph.ases.end()) {
        AS a;
        a.asn = origin_asn;
        graph.ases[origin_asn] = a;
    }

    AS& origin = graph.ases[origin_asn];

    if (!origin.policy) {
        origin.policy = new BGP();
    }

    Announcement seed;
    seed.prefix = "1.2.0.0/16";
    seed.next_hop_asn = 1;
    seed.as_path = std::vector<int>{1};

    seed.received_from = FROM_CUSTOMER;
    BGP* bgp = static_cast<BGP*>(origin.policy);
    bgp->installRoute(seed);    

    if (bgp->hasRoute("1.2.0.0/16")) {
        Announcement r = bgp->getRoute("1.2.0.0/16");
        std::cout << "Seeded route installed" << std::endl;
        std::cout << "  prefix: " << r.prefix << std::endl;
        std::cout << "  next hop: AS" << r.next_hop_asn << std::endl;
        std::cout << "  received_from: " << r.received_from << std::endl;
        std::cout << "  as_path: [";
        for (size_t i = 0; i < r.as_path.size(); ++i) {
            std::cout << r.as_path[i];
            if (i + 1 < r.as_path.size()) std::cout << " ";
        }
        std::cout << "]" << std::endl;
    } else {
        std::cout << "Seed route not found in local RIB" << std::endl;
    }

    return 0;
}
