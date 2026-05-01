// announcement_loader.cpp
// reads a CSV of route announcements and seeds them into the graph
// before propagation runs, each origin AS needs to have its routes installed
// this file handles that setup step
//
// expected CSV format (no spaces around commas):
//   asn,prefix,rov_invalid
//   15169,8.8.8.0/24,False
//   3356,1.2.3.0/24,True
//
// rov_invalid=True means the prefix/origin pair fails RPKI validation

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

#include "../include/graph.hpp"
#include "../include/as.hpp"
#include "../include/bgp_policy.hpp"
#include "../include/announcement.hpp"

// loadAnnouncements - parses the announcements CSV and installs each route
// into the originating AS's BGP policy table
//
// if an AS from the CSV doesn't exist in the graph yet, we create it here
// (this can happen if the AS relationship file doesn't include an origin AS)
//
// each announcement is seeded as if received from a customer (FROM_CUSTOMER)
// so it gets the highest local preference during propagation
void loadAnnouncements(Graph& graph, const std::string& filePath) {
    std::ifstream in(filePath.c_str());
    if (!in.is_open()) {
        std::cerr << "Could not open announcements file: " << filePath << std::endl;
        return;
    }

    std::string line;
    std::getline(in, line); // skip header row

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string token;

        // parse the three columns: asn, prefix, rov_invalid
        std::getline(ss, token, ',');
        int asn = std::stoi(token);

        std::getline(ss, token, ',');
        std::string prefix = token;

        std::getline(ss, token, ',');
        bool rov_invalid = (token == "True");

        // create the AS node if it isn't already in the graph
        if (graph.ases.find(asn) == graph.ases.end()) {
            AS a; a.asn = asn;
            graph.ases[asn] = a;
        }

        // make sure the AS has a BGP policy object to hold routes
        AS& node = graph.ases[asn];
        if (!node.policy) {
            node.policy = new BGP();
        }

        // build the announcement - origin AS starts the path with just itself
        Announcement ann;
        ann.prefix = prefix;
        ann.next_hop_asn = asn;
        ann.as_path = std::vector<int>{asn};
        ann.received_from = FROM_CUSTOMER; // seeds with highest preference
        ann.rov_invalid = rov_invalid;

        static_cast<BGP*>(node.policy)->installRoute(ann);
    }
}