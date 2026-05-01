// graph_loader.cpp
// reads the CAIDA AS relationship file and builds the AS graph
// the graph is what the rest of the simulator operates on
//
// input file format (pipe-delimited, lines starting with # are comments):
//   AS1|AS2|relationship|source
//   3356|15169|-1|bgp   -> 3356 is provider of 15169
//   3356|1299|0|bgp     -> 3356 and 1299 are peers
//
// relationship codes (defined in relationships.hpp):
//   -1 = provider to customer (AS1 provides transit to AS2)
//    0 = peer to peer (settlement-free, symmetric)

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

#include "../include/relationships.hpp"
#include "../include/graph.hpp"

// loadGraph - parses the AS relationship file and returns a populated Graph
// each line adds two AS nodes (if not already present) and links them
// with the appropriate relationship (customers/providers/peers lists)
//
// lines are skipped if they're empty or start with # (comments in the CAIDA file)
Graph loadGraph(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filepath << std::endl;
        return Graph();
    }

    Graph graph;
    std::string line;

    while (std::getline(file, line)) {
        // skip blank lines and comment lines (CAIDA file has a lot of header comments)
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        std::string token;

        // parse the first three pipe-delimited fields: as1, as2, relationship
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
            // AS1 is the provider, AS2 is the customer - add to both sides
            graph.ases[as1].customers.push_back(as2);
            graph.ases[as2].providers.push_back(as1);
        } else if (rel == PEER_TO_PEER) {
            // peer relationships are symmetric, so add to both sides
            graph.ases[as1].peers.push_back(as2);
            graph.ases[as2].peers.push_back(as1);
        }
        // any other relationship code is ignored
    }

    return graph;
}
