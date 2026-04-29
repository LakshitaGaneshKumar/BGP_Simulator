#include <cassert>
#include <iostream>
#include "../include/graph.hpp"

// no cycle: 1 -> 2 -> 3
void test_no_cycle() {
    Graph g;
    AS a; a.asn = 1; g.ases[1] = a;
    AS b; b.asn = 2; g.ases[2] = b;
    AS c; c.asn = 3; g.ases[3] = c;
    g.ases[1].customers.push_back(2); g.ases[2].providers.push_back(1);
    g.ases[2].customers.push_back(3); g.ases[3].providers.push_back(2);

    assert(hasCycle(g) == false);
    std::cout << "PASS: test_no_cycle" << std::endl;
}

// cycle: 1 -> 2 -> 3 -> 1
void test_with_cycle() {
    Graph g;
    AS a; a.asn = 1; g.ases[1] = a;
    AS b; b.asn = 2; g.ases[2] = b;
    AS c; c.asn = 3; g.ases[3] = c;
    g.ases[1].customers.push_back(2); g.ases[2].providers.push_back(1);
    g.ases[2].customers.push_back(3); g.ases[3].providers.push_back(2);
    g.ases[3].customers.push_back(1); g.ases[1].providers.push_back(3); // cycle

    assert(hasCycle(g) == true);
    std::cout << "PASS: test_with_cycle" << std::endl;
}

void test_peer_cycle_ignored() {
    Graph g;
    AS a; a.asn = 1; g.ases[1] = a;
    AS b; b.asn = 2; g.ases[2] = b;
    g.ases[1].peers.push_back(2);
    g.ases[2].peers.push_back(1);

    assert(hasCycle(g) == false);
    std::cout << "PASS: test_peer_cycle_ignored" << std::endl;
}

int main() {
    test_no_cycle();
    test_with_cycle();
    test_peer_cycle_ignored();
    std::cout << "All cycle tests passed." << std::endl;
    return 0;
}