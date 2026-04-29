#include <map>
#include <vector>
#include "../include/graph.hpp"

// DFS helper - follows the customer edges (provider -> customer direction)
static bool dfs(int asn, const Graph& graph, std::map<int, bool>& visited, std::map<int, bool>& inStack) {
    visited[asn] = true;
    inStack[asn] = true;

    const AS& node = graph.ases.at(asn);
    for (int i = 0; i < (int)node.customers.size(); i++) {
        int customer = node.customers[i];

        if (!visited[customer]) {
            if (dfs(customer, graph, visited, inStack)) {
                return true;
            }
        } else if (inStack[customer]) {
            return true;
        }
    }

    inStack[asn] = false;
    return false;
}

// Returns true if there is a provider/customer cycle in the graph
bool hasCycle(const Graph& graph) {
    std::map<int, bool> visited;
    std::map<int, bool> inStack;

    // initialize all nodes as not visited
    for (auto it = graph.ases.begin(); it != graph.ases.end(); it++) {
        visited[it->first] = false;
        inStack[it->first] = false;
    }

    for (auto it = graph.ases.begin(); it != graph.ases.end(); it++) {
        if (!visited[it->first]) {
            if (dfs(it->first, graph, visited, inStack)) {
                return true;
            }
        }
    }

    return false;
}