// test_cycle.cpp
// unit tests for hasCycle() in cycle_detection.cpp
// hasCycle() only looks at provider->customer edges (the directed hierarchy),
// so peer relationships should never contribute to a detected cycle.
// these tests cover the three main cases: clean DAG, actual cycle, peers ignored.

#include <cassert>
#include <iostream>
#include "../include/graph.hpp"

// test_no_cycle - simple linear chain: 1 is provider of 2, 2 is provider of 3
// this is a valid DAG, hasCycle should return false
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

// test_with_cycle - same chain but 3 is also a provider of 1, closing the loop
// 1 -> 2 -> 3 -> 1 is a cycle in the provider/customer hierarchy
// hasCycle should catch the back edge and return true
void test_with_cycle() {
    Graph g;
    AS a; a.asn = 1; g.ases[1] = a;
    AS b; b.asn = 2; g.ases[2] = b;
    AS c; c.asn = 3; g.ases[3] = c;
    g.ases[1].customers.push_back(2); g.ases[2].providers.push_back(1);
    g.ases[2].customers.push_back(3); g.ases[3].providers.push_back(2);
    g.ases[3].customers.push_back(1); g.ases[1].providers.push_back(3); // closes the cycle

    assert(hasCycle(g) == true);
    std::cout << "PASS: test_with_cycle" << std::endl;
}

// test_peer_cycle_ignored - two ASes that are mutual peers form a cycle in the
// undirected sense, but hasCycle only traverses provider->customer edges,
// so this should not be flagged as a cycle
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