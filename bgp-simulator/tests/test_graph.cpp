#include <cassert>
#include <iostream>
#include <fstream>
#include "../include/graph.hpp"

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

void test_provider_customer() {
    Graph g = buildGraph(1, 2, -1);
    assert(g.ases[1].customers[0] == 2);
    assert(g.ases[1].providers.size() == 0);
    assert(g.ases[2].providers[0] == 1);
    assert(g.ases[2].customers.size() == 0);
    std::cout << "PASS: test_provider_customer" << std::endl;
}

void test_peer() {
    Graph g = buildGraph(10, 20, 0);
    assert(g.ases[10].peers[0] == 20);
    assert(g.ases[20].peers[0] == 10);
    assert(g.ases[10].providers.size() == 0);
    assert(g.ases[10].customers.size() == 0);
    std::cout << "PASS: test_peer" << std::endl;
}

void test_load_small_file() {
    std::ofstream f("test_input.txt");
    f << "# comment\n";
    f << "1|2|-1|bgp\n";
    f << "2|3|-1|bgp\n";
    f << "1|4|0|bgp\n";
    f.close();

    Graph g = loadGraph("test_input.txt");
    assert(g.ases.size() == 4);
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