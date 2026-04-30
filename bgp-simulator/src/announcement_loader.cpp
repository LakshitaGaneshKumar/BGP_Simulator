#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

#include "../include/graph.hpp"
#include "../include/as.hpp"
#include "../include/bgp_policy.hpp"
#include "../include/announcement.hpp"

void loadAnnouncements(Graph& graph, const std::string& filePath) {
    std::ifstream in(filePath.c_str());
    if (!in.is_open()) {
        std::cerr << "Could not open announcements file: " << filePath << std::endl;
        return;
    }

    std::string line;
    std::getline(in, line);

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string token;

        std::getline(ss, token, ',');
        int asn = std::stoi(token);

        std::getline(ss, token, ',');
        std::string prefix = token;

        std::getline(ss, token, ',');
        bool rov_invalid = (token == "True");

        if (graph.ases.find(asn) == graph.ases.end()) {
            AS a; a.asn = asn;
            graph.ases[asn] = a;
        }

        AS& node = graph.ases[asn];
        if (!node.policy) {
            node.policy = new BGP();
        }

        Announcement ann;
        ann.prefix = prefix;
        ann.next_hop_asn = asn;
        ann.as_path = std::vector<int>{asn};
        ann.received_from = FROM_CUSTOMER;
        ann.rov_invalid = rov_invalid;

        static_cast<BGP*>(node.policy)->installRoute(ann);
    }
}