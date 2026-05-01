// propagation_rank.cpp
// computes the order in which ASes should propagate routes
// BGP routes travel from customers up to providers, so we need to process
// ASes bottom-up: stubs first, then transit ASes, then large providers last
//
// this uses a modified topological sort (Kahn's algorithm) on the
// provider->customer DAG to assign each AS a rank:
//   rank 0 = stub/leaf ASes (no customers)
//   rank 1 = ASes whose only customers are rank-0
//   rank N = ASes that sit above rank N-1 ASes
//
// the simulator then propagates rank 0 first, then rank 1, etc.
// this guarantees every AS has already received routes from its customers
// before it decides what to send to its providers and peers

#include <map>
#include <queue>
#include <vector>
#include "../include/graph.hpp"

// buildPropagationRanks - assigns each AS a propagation rank and returns
// a 2D vector where ranks[r] is the list of ASNs at rank r
//
// uses a BFS starting from leaf nodes (no customers) and working upward
// through providers, similar to how topological sort processes a DAG
std::vector<std::vector<int> > buildPropagationRanks(Graph& graph) {
    std::map<int, int> customers_left; // tracks how many customers haven't been ranked yet
    std::queue<int> q;
    int max_rank = 0;

    // initialize: rank -1 means unranked; seed the queue with leaf nodes (rank 0)
    for (std::map<int, AS>::iterator it = graph.ases.begin(); it != graph.ases.end(); ++it) {
        int asn = it->first;
        AS& node = it->second;
        node.propagation_rank = -1;
        customers_left[asn] = (int)node.customers.size();

        // leaf ASes have no customers, so they're ready to rank immediately
        if (customers_left[asn] == 0) {
            node.propagation_rank = 0;
            q.push(asn);
        }
    }

    // BFS upward through the provider links
    // a provider's rank = max(its customers' ranks) + 1
    while (!q.empty()) {
        int current = q.front();
        q.pop();

        AS& cur_node = graph.ases[current];
        int cur_rank = cur_node.propagation_rank;
        if (cur_rank > max_rank) {
            max_rank = cur_rank;
        }

        for (int i = 0; i < (int)cur_node.providers.size(); i++) {
            int provider_asn = cur_node.providers[i];
            AS& provider = graph.ases[provider_asn];

            // push the provider's rank up if this customer is higher than expected
            if (provider.propagation_rank < cur_rank + 1) {
                provider.propagation_rank = cur_rank + 1;
            }

            // decrement the "waiting on customers" counter for this provider
            // when it hits 0, all its customers are ranked so we can enqueue it
            customers_left[provider_asn]--;

            if (customers_left[provider_asn] == 0) {
                q.push(provider_asn);
            }
        }
    }

    // bucket ASes by rank into a 2D vector for the simulator to iterate over
    std::vector<std::vector<int> > ranks(max_rank + 1);
    for (std::map<int, AS>::iterator it = graph.ases.begin(); it != graph.ases.end(); ++it) {
        int asn = it->first;
        int r = it->second.propagation_rank;
        if (r >= 0) {
            ranks[r].push_back(asn);
        }
        // r == -1 means the AS was never reachable from any leaf (shouldn't happen if no cycles)
    }

    return ranks;
}
