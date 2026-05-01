// cycle_detection.cpp
// checks the AS graph for cycles in the provider->customer hierarchy
// a cycle here means something like: AS A is a provider of AS B,
// which is a provider of AS C, which is somehow a provider of AS A
//
// this shouldn't happen in real CAIDA data but we check anyway before
// running propagation, since a cycle would cause infinite loops
// uses standard iterative DFS with a "currently in stack" marker

#include <map>
#include <vector>
#include "../include/graph.hpp"

// dfs - recursive DFS helper that follows provider->customer edges
// visited = has this node been seen at all
// inStack = is this node currently on the active DFS call stack
// if we reach a node that's already on the stack, we found a cycle
static bool dfs(int asn, const Graph& graph, std::map<int, bool>& visited, std::map<int, bool>& inStack) {
    visited[asn] = true;
    inStack[asn] = true;

    const AS& node = graph.ases.at(asn);
    for (int i = 0; i < (int)node.customers.size(); i++) {
        int customer = node.customers[i];

        if (!visited[customer]) {
            // haven't seen this customer yet, recurse into it
            if (dfs(customer, graph, visited, inStack)) {
                return true;
            }
        } else if (inStack[customer]) {
            // already on the stack = back edge = cycle
            return true;
        }
    }

    // done with this node, pop it off the stack
    inStack[asn] = false;
    return false;
}

// hasCycle - returns true if there is a provider/customer cycle anywhere in the graph
// runs DFS from every unvisited node so disconnected components are covered too
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