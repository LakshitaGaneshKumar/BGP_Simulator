#include <fstream>
#include <iostream>
#include <string>
#include <map>
#include <vector>

#include "../include/graph.hpp"
#include "../include/as.hpp"
#include "../include/bgp_policy.hpp"
#include "../include/announcement.hpp"

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

        const BGP* bgp = dynamic_cast<const BGP*>(node.policy);
        if (!bgp) {
            continue;
        }

        for (std::map<std::string, Announcement>::const_iterator rt = bgp->local_rib.begin();
             rt != bgp->local_rib.end(); ++rt) {
            const Announcement& ann = rt->second;

            out << node.asn << "," << ann.prefix << ",\"";
            for (size_t i = 0; i < ann.as_path.size(); ++i) {
                if (i > 0) out << " ";
                out << ann.as_path[i];
            }
            out << "\"\n";
        }
    }
}