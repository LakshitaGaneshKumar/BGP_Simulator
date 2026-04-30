#include <fstream>
#include <iostream>
#include <string>

#include "../include/rov_policy.hpp"
#include "../include/as.hpp"

void applyRovPolicies(Graph& graph, const std::string& rovFilePath) {
    std::ifstream in(rovFilePath.c_str());
    if (!in.is_open()) {
        std::cerr << "Could not open ROV file: " << rovFilePath << std::endl;
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        int asn = std::stoi(line);

        if (graph.ases.find(asn) == graph.ases.end()) {
            continue;
        }

        AS& node = graph.ases[asn];
        if (node.policy) {
            delete node.policy;
            node.policy = 0;
        }
        node.policy = new ROV();
    }
}