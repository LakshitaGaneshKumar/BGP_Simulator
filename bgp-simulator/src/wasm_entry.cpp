// wasm_entry.cpp
// this is the WebAssembly entry point — compiled with em++ instead of g++
// (see main.cpp for the equivalent native CLI entry point)
//
// instead of reading files from disk, everything is passed in as strings
// because the browser can't do filesystem I/O the way a native program can.
// Emscripten's embind exposes runSimulation() to JavaScript via the
// BGPSimModule object defined in bgp_sim.js.
//
// call flow from the browser side:
//   BGPSimModule().then(m => {
//     const csvResult = m.runSimulation(asRelStr, annsStr, rovStr);
//   });
//
// the three input strings mirror what the CLI version reads from files:
//   asRelContent  - CAIDA AS relationship file (pipe-delimited, same format as 20260401.as-rel2.txt)
//   annsContent   - announcements CSV (asn, prefix, rov_invalid)
//   rovContent    - optional list of ROV-enforcing ASNs, one per line (empty string = no ROV)

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

// loadGraphFromString - parses a CAIDA AS relationship file from a string
// and builds the AS graph (same logic as graph_loader.cpp but reads from memory)
// format per line: as1|as2|relationship
//   -1 = as1 is provider of as2 (PROVIDER_TO_CUSTOMER)
//    0 = as1 and as2 are peers    (PEER_TO_PEER)
static Graph loadGraphFromString(const std::string& content) {
    Graph graph;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue; // skip blank lines and comments

        std::stringstream ss(line);
        std::string token;

        std::getline(ss, token, '|');
        int as1 = std::stoi(token);

        std::getline(ss, token, '|');
        int as2 = std::stoi(token);

        std::getline(ss, token, '|');
        int rel = std::stoi(token);

        // create AS nodes if they don't exist yet
        if (graph.ases.find(as1) == graph.ases.end()) {
            AS a; a.asn = as1; graph.ases[as1] = a;
        }
        if (graph.ases.find(as2) == graph.ases.end()) {
            AS a; a.asn = as2; graph.ases[as2] = a;
        }

        // wire up the bidirectional relationship
        if (rel == PROVIDER_TO_CUSTOMER) {
            graph.ases[as1].customers.push_back(as2);
            graph.ases[as2].providers.push_back(as1);
        } else if (rel == PEER_TO_PEER) {
            graph.ases[as1].peers.push_back(as2);
            graph.ases[as2].peers.push_back(as1);
        }
        // unknown rel codes are silently ignored
    }

    return graph;
}

// applyRovPoliciesFromString - reads a list of ASNs (one per line) and upgrades
// each matching AS from standard BGP to ROV (Route Origin Validation)
// ROV ASes will reject announcements marked rov_invalid=True during propagation
// must be called BEFORE loadAnnouncementsFromString so ROV filters apply correctly
static void applyRovPoliciesFromString(Graph& graph, const std::string& content) {
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;

        int asn = std::stoi(line);
        if (graph.ases.find(asn) == graph.ases.end()) continue; // AS not in graph, skip

        AS& node = graph.ases[asn];
        // replace the existing policy (delete to avoid memory leak)
        if (node.policy) {
            delete node.policy;
            node.policy = 0;
        }
        node.policy = new ROV();
    }
}

// loadAnnouncementsFromString - reads the announcements CSV from a string and seeds
// each origin AS's local RIB with the announced prefixes
// format: asn,prefix,rov_invalid
//   rov_invalid is "True" or "False" — used by ROV ASes to filter invalid routes
static void loadAnnouncementsFromString(Graph& graph, const std::string& content) {
    std::istringstream stream(content);
    std::string line;

    std::getline(stream, line); // skip header row (asn,prefix,rov_invalid)

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

        // create the AS if it wasn't in the relationship file (edge case)
        if (graph.ases.find(asn) == graph.ases.end()) {
            AS a; a.asn = asn; graph.ases[asn] = a;
        }

        AS& node = graph.ases[asn];
        if (!node.policy) {
            node.policy = new BGP(); // give origin AS a default BGP policy if none set
        }

        // seed the announcement directly into the local RIB as a self-originated route
        Announcement ann;
        ann.prefix = prefix;
        ann.next_hop_asn = asn;
        ann.as_path = std::vector<int>(1, asn); // path starts with just the origin
        ann.received_from = FROM_CUSTOMER;       // treated as customer-learned for preference purposes
        ann.rov_invalid = rov_invalid;

        static_cast<BGP*>(node.policy)->installRoute(ann);
    }
}

// dumpGraphToCsvString - serializes the post-propagation local RIBs of all ASes
// into a CSV string to send back to JavaScript
// output format: asn,prefix,as_path
//   as_path is quoted and Python-tuple-style, e.g. "(1, 2, 3)" or "(1,)" for single-hop
static std::string dumpGraphToCsvString(const Graph& graph) {
    std::ostringstream out;
    out << "asn,prefix,as_path\n";

    for (std::map<int, AS>::const_iterator it = graph.ases.begin();
         it != graph.ases.end(); ++it) {
        const AS& node = it->second;
        if (!node.policy) continue; // AS has no routes, skip

        // dynamic_cast handles ROV subclass too — both store routes in local_rib via BGP base
        const BGP* bgp = dynamic_cast<const BGP*>(node.policy);
        if (!bgp) continue;

        for (std::map<std::string, Announcement>::const_iterator rt = bgp->local_rib.begin();
             rt != bgp->local_rib.end(); ++rt) {
            const Announcement& ann = rt->second;

            out << node.asn << "," << ann.prefix << ",\"(";
            if (ann.as_path.size() == 1) {
                // single-element tuple needs trailing comma to match Python tuple format
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

// runSimulation - main entry point exposed to JavaScript via embind
// runs the full pipeline: load graph -> cycle check -> apply ROV -> seed announcements
//                         -> rank -> propagate -> serialize results
// returns a CSV string on success, or an "ERROR: ..." string if something fails
std::string runSimulation(
    const std::string& asRelContent,
    const std::string& annsContent,
    const std::string& rovContent
) {
    Graph graph = loadGraphFromString(asRelContent);

    // bail early if the topology has a cycle — propagation ranks require a DAG
    if (hasCycle(graph)) {
        return "ERROR: provider/customer cycle detected in the graph.";
    }

    // ROV must be applied before announcements so the policy is in place when routes are installed
    applyRovPoliciesFromString(graph, rovContent);
    loadAnnouncementsFromString(graph, annsContent);

    std::vector<std::vector<int> > ranks = buildPropagationRanks(graph);
    propagateAnnouncements(graph, ranks);

    return dumpGraphToCsvString(graph);
}

// expose runSimulation to JavaScript under the BGPSimModule namespace
EMSCRIPTEN_BINDINGS(bgp_sim) {
    emscripten::function("runSimulation", &runSimulation);
}