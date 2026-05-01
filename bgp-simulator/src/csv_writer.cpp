// csv_writer.cpp
// after the simulation finishes propagating routes, this writes the results
// to a CSV file so they can be downloaded or further analyzed
//
// output format:
//   asn,prefix,as_path
//   701,1.2.3.0/24,"(701, 3356, 15169)"
//
// each row is one prefix that an AS learned during propagation
// the as_path column is quoted because it contains commas

#include <fstream>
#include <iostream>
#include <string>
#include <map>
#include <vector>

#include "../include/graph.hpp"
#include "../include/as.hpp"
#include "../include/bgp_policy.hpp"
#include "../include/announcement.hpp"

// dumpGraphToCsv - iterates over every AS in the graph and writes
// all the routes in its local RIB to the output CSV
//
// skips any AS that has no policy (shouldn't happen after a full run,
// but safe to check) and any AS whose policy isn't a BGP instance
void dumpGraphToCsv(const Graph& graph, const std::string& outputPath) {
    std::ofstream out(outputPath.c_str());
    if (!out.is_open()) {
        std::cerr << "Could not open output file: " << outputPath << std::endl;
        return;
    }

    out << "asn,prefix,as_path\n";

    for (std::map<int, AS>::const_iterator it = graph.ases.begin(); it != graph.ases.end(); ++it) {
        const AS& node = it->second;
        if (!node.policy) {
            continue;
        }

        // dynamic_cast returns null if the policy isn't actually a BGP object
        const BGP* bgp = dynamic_cast<const BGP*>(node.policy);
        if (!bgp) {
            continue;
        }

        // local_rib maps prefix -> best announcement for that prefix
        for (std::map<std::string, Announcement>::const_iterator rt = bgp->local_rib.begin();
             rt != bgp->local_rib.end(); ++rt) {
            const Announcement& ann = rt->second;

            // write: asn,prefix,"(a, b, c)"
            // single-hop paths get a trailing comma to match Python tuple format: (701,)
            out << node.asn << "," << ann.prefix << ",\"(";
            if (ann.as_path.size() == 1) {
                out << ann.as_path[0] << ",";
            } else {
                for (size_t i = 0; i < ann.as_path.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << ann.as_path[i];
                }
            }
            out << ")\"\n";
        }
    }
}