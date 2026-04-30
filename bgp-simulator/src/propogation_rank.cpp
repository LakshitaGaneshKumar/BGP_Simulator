#include <map>
#include <queue>
#include <vector>
#include "../include/graph.hpp"

std::vector<std::vector<int> > buildPropagationRanks(Graph& graph) {
    std::map<int, int> customers_left;
    std::queue<int> q;
    int max_rank = 0;

    for (std::map<int, AS>::iterator it = graph.ases.begin(); it != graph.ases.end(); ++it) {
        int asn = it->first;
        AS& node = it->second;
        node.propagation_rank = -1;
        customers_left[asn] = (int)node.customers.size();

        if (customers_left[asn] == 0) {
            node.propagation_rank = 0;
            q.push(asn);
        }
    }

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

            if (provider.propagation_rank < cur_rank + 1) {
                provider.propagation_rank = cur_rank + 1;
            }

            customers_left[provider_asn]--;

            if (customers_left[provider_asn] == 0) {
                q.push(provider_asn);
            }
        }
    }

    std::vector<std::vector<int> > ranks(max_rank + 1);
    for (std::map<int, AS>::iterator it = graph.ases.begin(); it != graph.ases.end(); ++it) {
        int asn = it->first;
        int r = it->second.propagation_rank;
        if (r >= 0) {
            ranks[r].push_back(asn);
        }
    }

    return ranks;
}