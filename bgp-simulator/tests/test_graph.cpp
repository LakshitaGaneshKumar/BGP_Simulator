// test_graph.cpp
// unit tests for graph construction and the loadGraph() function
// checks that provider/customer and peer relationships are wired up correctly
// in both directions, and that the file parser handles comments and multiple
// relationship types in one file.

#include <cassert>
#include <iostream>
#include <fstream>
#include "../include/graph.hpp"

// buildGraph - helper to create a minimal two-AS graph with a given relationship code
// rel == -1: as1 is provider of as2 (PROVIDER_TO_CUSTOMER)
// rel ==  0: as1 and as2 are peers  (PEER_TO_PEER)
static Graph buildGraph(int as1, int as2, int rel) {
    Graph g;
    AS a; a.asn = as1; g.ases[as1] = a;
    AS b; b.asn = as2; g.ases[as2] = b;
    if (rel == -1) {
        g.ases[as1].customers.push_back(as2);
        g.ases[as2].providers.push_back(as1);
    } else if (rel == 0) {
        g.ases[as1].peers.push_back(as2);
        g.ases[as2].peers.push_back(as1);
    }
    return g;
}

// test_provider_customer - rel=-1 should make AS1 the provider and AS2 the customer
// checks both directions: AS1 has AS2 in customers, AS2 has AS1 in providers
// also verifies that no extra relationships were added
void test_provider_customer() {
    Graph g = buildGraph(1, 2, -1);
    assert(g.ases[1].customers[0] == 2);
    assert(g.ases[1].providers.size() == 0);
    assert(g.ases[2].providers[0] == 1);
    assert(g.ases[2].customers.size() == 0);
    std::cout << "PASS: test_provider_customer" << std::endl;
}

// test_peer - rel=0 should make both ASes peers of each other (symmetric)
// peers don't have provider/customer relationships so those lists should be empty
void test_peer() {
    Graph g = buildGraph(10, 20, 0);
    assert(g.ases[10].peers[0] == 20);
    assert(g.ases[20].peers[0] == 10);
    assert(g.ases[10].providers.size() == 0);
    assert(g.ases[10].customers.size() == 0);
    std::cout << "PASS: test_peer" << std::endl;
}

// test_load_small_file - writes a small AS relationship file to disk and loads it
// via loadGraph() to verify the file parser works end-to-end
// the file has a comment line (should be skipped), two provider/customer edges,
// and one peer edge — all mixed together as they would be in a real CAIDA file
void test_load_small_file() {
    std::ofstream f("test_input.txt");
    f << "# comment\n";       // should be skipped by the parser
    f << "1|2|-1|bgp\n";      // AS1 is provider of AS2
    f << "2|3|-1|bgp\n";      // AS2 is provider of AS3
    f << "1|4|0|bgp\n";       // AS1 and AS4 are peers
    f.close();

    Graph g = loadGraph("test_input.txt");
    assert(g.ases.size() == 4);            // 4 distinct ASes should be created
    assert(g.ases[1].customers[0] == 2);
    assert(g.ases[2].providers[0] == 1);
    assert(g.ases[2].customers[0] == 3);
    assert(g.ases[1].peers[0] == 4);
    assert(g.ases[4].peers[0] == 1);
    std::cout << "PASS: test_load_small_file" << std::endl;
}

int main() {
    test_provider_customer();
    test_peer();
    test_load_small_file();
    std::cout << "All graph tests passed." << std::endl;
    return 0;
}