#include <emscripten/bind.h>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include "../include/graph.hpp"
#include "../include/as.hpp"
#include "../include/announcement.hpp"
#include "../include/bgp_policy.hpp"
#include "../include/rov_policy.hpp"
#include "../include/simulator.hpp"
#include "../include/relationships.hpp"

static Graph loadGraphFromString(const std::string& content) {
    Graph graph;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string token;

        std::getline(ss, token, '|');
        int as1 = std::stoi(token);

        std::getline(ss, token, '|');
        int as2 = std::stoi(token);

        std::getline(ss, token, '|');
        int rel = std::stoi(token);

        if (graph.ases.find(as1) == graph.ases.end()) {
            AS a; a.asn = as1; graph.ases[as1] = a;
        }
        if (graph.ases.find(as2) == graph.ases.end()) {
            AS a; a.asn = as2; graph.ases[as2] = a;
        }

        if (rel == PROVIDER_TO_CUSTOMER) {
            graph.ases[as1].customers.push_back(as2);
            graph.ases[as2].providers.push_back(as1);
        } else if (rel == PEER_TO_PEER) {
            graph.ases[as1].peers.push_back(as2);
            graph.ases[as2].peers.push_back(as1);
        }
    }

    return graph;
}

static void applyRovPoliciesFromString(Graph& graph, const std::string& content) {
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;

        int asn = std::stoi(line);
        if (graph.ases.find(asn) == graph.ases.end()) continue;

        AS& node = graph.ases[asn];
        if (node.policy) {
            delete node.policy;
            node.policy = 0;
        }
        node.policy = new ROV();
    }
}

static void loadAnnouncementsFromString(Graph& graph, const std::string& content) {
    std::istringstream stream(content);
    std::string line;

    std::getline(stream, line); // skip header

    while (std::getline(stream, line)) {
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
            AS a; a.asn = asn; graph.ases[asn] = a;
        }

        AS& node = graph.ases[asn];
        if (!node.policy) {
            node.policy = new BGP();
        }

        Announcement ann;
        ann.prefix = prefix;
        ann.next_hop_asn = asn;
        ann.as_path = std::vector<int>(1, asn);
        ann.received_from = FROM_CUSTOMER;
        ann.rov_invalid = rov_invalid;

        static_cast<BGP*>(node.policy)->installRoute(ann);
    }
}

static std::string dumpGraphToCsvString(const Graph& graph) {
    std::ostringstream out;
    out << "asn,prefix,as_path\n";

    for (std::map<int, AS>::const_iterator it = graph.ases.begin();
         it != graph.ases.end(); ++it) {
        const AS& node = it->second;
        if (!node.policy) continue;

        const BGP* bgp = dynamic_cast<const BGP*>(node.policy);
        if (!bgp) continue;

        for (std::map<std::string, Announcement>::const_iterator rt = bgp->local_rib.begin();
             rt != bgp->local_rib.end(); ++rt) {
            const Announcement& ann = rt->second;

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

    return out.str();
}

std::string runSimulation(
    const std::string& asRelContent,
    const std::string& annsContent,
    const std::string& rovContent
) {
    Graph graph = loadGraphFromString(asRelContent);

    if (hasCycle(graph)) {
        return "ERROR: provider/customer cycle detected in the graph.";
    }

    applyRovPoliciesFromString(graph, rovContent);
    loadAnnouncementsFromString(graph, annsContent);

    std::vector<std::vector<int> > ranks = buildPropagationRanks(graph);
    propagateAnnouncements(graph, ranks);

    return dumpGraphToCsvString(graph);
}

EMSCRIPTEN_BINDINGS(bgp_sim) {
    emscripten::function("runSimulation", &runSimulation);
}