#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

#include "../include/relationships.hpp"
#include "../include/graph.hpp"

// Each line in the file looks like: AS1|AS2|relationship|source
// relationship = -1 means AS1 is provider, AS2 is customer
// relationship =  0 means AS1 and AS2 are peers
Graph loadGraph(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filepath << std::endl;
        return Graph();
    }

    Graph graph;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        std::string token;

        std::getline(ss, token, '|');
        int as1 = std::stoi(token);

        std::getline(ss, token, '|');
        int as2 = std::stoi(token);

        std::getline(ss, token, '|');
        int rel = std::stoi(token);

        // create AS entries if they don't exist yet
        if (graph.ases.find(as1) == graph.ases.end()) {
            AS a;
            a.asn = as1;
            graph.ases[as1] = a;
        }
        if (graph.ases.find(as2) == graph.ases.end()) {
            AS a;
            a.asn = as2;
            graph.ases[as2] = a;
        }

        if (rel == PROVIDER_TO_CUSTOMER) {
            // AS1 is the provider, AS2 is the customer
            graph.ases[as1].customers.push_back(as2);
            graph.ases[as2].providers.push_back(as1);
        } else if (rel == PEER_TO_PEER) {
            graph.ases[as1].peers.push_back(as2);
            graph.ases[as2].peers.push_back(as1);
        }
    }

    return graph;
}
