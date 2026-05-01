// rov_loader.cpp
// reads a list of ASNs that enforce Route Origin Validation (ROV)
// and upgrades their policy from BGP to ROV
//
// ROV is a security extension where an AS checks incoming routes against
// RPKI records and drops any route where the origin AS doesn't match
// the registered prefix owner (i.e. rov_invalid=True routes get filtered)
//
// input file format: one ASN per line, blank lines and # comments are ignored
//   # these ASes enforce ROV
//   3356
//   15169
//
// this must run before loadAnnouncements so that when routes arrive
// at an ROV-enforcing AS, the ROV policy object is already in place

#include <fstream>
#include <iostream>
#include <string>

#include "../include/rov_policy.hpp"
#include "../include/as.hpp"

// applyRovPolicies - reads the ROV ASN file and replaces the policy on each
// listed AS with an ROV instance instead of the default BGP policy
//
// if an ASN from the file doesn't exist in the graph, it's silently skipped
// (the AS might not appear in the CAIDA relationship data)
void applyRovPolicies(Graph& graph, const std::string& rovFilePath) {
    std::ifstream in(rovFilePath.c_str());
    if (!in.is_open()) {
        std::cerr << "Could not open ROV file: " << rovFilePath << std::endl;
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        // skip blank lines and comment lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        int asn = std::stoi(line);

        // skip ASNs that aren't in the loaded graph
        if (graph.ases.find(asn) == graph.ases.end()) {
            continue;
        }

        AS& node = graph.ases[asn];

        // free the existing policy (usually null at this point, but be safe)
        // then assign an ROV policy which will filter rov_invalid routes
        if (node.policy) {
            delete node.policy;
            node.policy = 0;
        }
        node.policy = new ROV();
    }
}
