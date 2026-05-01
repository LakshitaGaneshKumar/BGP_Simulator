// main.cpp
// command-line entry point for the BGP simulator
// this is only used when compiling natively (not for the wasm build)
// the wasm build uses wasm_entry.cpp instead
//
// usage:
//   ./bgp_sim <as-rel2-file> <anns.csv> <rov_asns.csv> <output.csv>
//
//   as-rel2-file  - CAIDA AS relationship file (pipe-delimited)
//   anns.csv      - announcements to seed (asn,prefix,rov_invalid)
//   rov_asns.csv  - ASNs that enforce ROV (can be empty file)
//   output.csv    - where to write the propagation results
//
// pipeline: load graph -> check for cycles -> apply ROV -> load announcements
//        -> compute propagation order -> propagate -> write output

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

    // step 1: parse the CAIDA file and build the AS graph
    Graph graph = loadGraph(argv[1]);

    // step 2: sanity check - a cycle in provider/customer links would
    // cause propagation to loop forever, so we abort early if one is found
    if (hasCycle(graph)) {
        std::cerr << "Error: provider/customer cycle detected in the graph." << std::endl;
        return 1;
    }

    // step 3: mark ROV-enforcing ASes before seeding announcements
    // (ROV needs to be set up before routes arrive so filtering works correctly)
    applyRovPolicies(graph, argv[3]);

    // step 4: seed origin ASes with their announced prefixes
    loadAnnouncements(graph, argv[2]);

    // step 5: compute propagation order (customer -> peer -> provider ranking)
    // and run the actual route propagation
    std::vector<std::vector<int> > ranks = buildPropagationRanks(graph);
    propagateAnnouncements(graph, ranks);

    // step 6: write every AS's best route for each prefix to the output CSV
    dumpGraphToCsv(graph, argv[4]);
    std::cout << "Wrote " << argv[4] << std::endl;

    return 0;
}
