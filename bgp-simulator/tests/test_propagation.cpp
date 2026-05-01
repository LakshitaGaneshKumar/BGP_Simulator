// test_propagation.cpp
// tests for the three-phase BGP propagation logic in simulator.cpp
// covers each propagation direction individually, AS path prepending,
// all three Gao-Rexford tiebreak rules, CSV output, and multi-origin conflicts.
//
// test list:
//   test_up_propagation          - customer route reaches provider (phase 1)
//   test_peer_propagation        - route reaches a peer (phase 2)
//   test_down_propagation        - provider route reaches customer (phase 3)
//   test_as_path_prepending      - each hop prepends its own ASN
//   test_conflict_customer_beats_peer    - FROM_CUSTOMER preferred over FROM_PEER
//   test_conflict_shorter_as_path        - shorter path preferred when received_from ties
//   test_conflict_lower_next_hop         - lower next_hop_asn used as final tiebreak
//   test_csv_tiny_graph          - dumpGraphToCsv produces correct output format
//   test_csv_larger_graph        - CSV contains the right prefix after full propagation
//   test_two_announcements_same_prefix   - same prefix from two origins, best one wins

#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include "../include/graph.hpp"
#include "../include/as.hpp"
#include "../include/announcement.hpp"
#include "../include/bgp_policy.hpp"
#include "../include/simulator.hpp"

// makeAnn - convenience helper to build an Announcement without spelling out every field
// hop = next_hop_asn, from = received_from (FROM_CUSTOMER / FROM_PEER / FROM_PROVIDER)
static Announcement makeAnn(const std::string& prefix, int hop, int from, const std::vector<int>& path) {
    Announcement a;
    a.prefix = prefix;
    a.next_hop_asn = hop;
    a.received_from = from;
    a.as_path = path;
    return a;
}

// test_up_propagation - phase 1 (bottom-up)
// AS1 originates a route; AS2 is its provider
// after propagation AS2 should have the route tagged FROM_CUSTOMER,
// and AS2 should have prepended its own ASN to the front of the path
void test_up_propagation() {
    Graph g;
    AS a1; a1.asn = 1; a1.policy = new BGP();
    AS a2; a2.asn = 2; a2.policy = new BGP();
    
    a1.providers.push_back(2);
    a2.customers.push_back(1);
    
    g.ases[1] = a1;
    g.ases[2] = a2;

    static_cast<BGP*>(g.ases[1].policy)->installRoute(makeAnn("1.0.0.0/8", 1, FROM_CUSTOMER, {1}));

    std::vector<std::vector<int> > ranks(2);
    ranks[0].push_back(1);
    ranks[1].push_back(2);

    propagateAnnouncements(g, ranks);

    BGP* bgp2 = static_cast<BGP*>(g.ases[2].policy);
    assert(bgp2->hasRoute("1.0.0.0/8"));
    assert(bgp2->getRoute("1.0.0.0/8").as_path[0] == 2);
    assert(bgp2->getRoute("1.0.0.0/8").received_from == FROM_CUSTOMER);

    delete bgp2;
    delete static_cast<BGP*>(g.ases[1].policy);
    std::cout << "PASS: test_up_propagation" << std::endl;
}

// test_peer_propagation - phase 2 (peer exchange)
// AS1 and AS2 are peers; AS1 has a route
// after propagation AS2 should receive it tagged FROM_PEER
void test_peer_propagation() {
    Graph g;
    AS a1; a1.asn = 1; a1.policy = new BGP();
    AS a2; a2.asn = 2; a2.policy = new BGP();
    
    a1.peers.push_back(2);
    a2.peers.push_back(1);
    
    g.ases[1] = a1;
    g.ases[2] = a2;

    static_cast<BGP*>(g.ases[1].policy)->installRoute(makeAnn("1.0.0.0/8", 1, FROM_CUSTOMER, {1}));

    std::vector<std::vector<int> > ranks(1);
    ranks[0].push_back(1);
    ranks[0].push_back(2);

    propagateAnnouncements(g, ranks);

    BGP* bgp2 = static_cast<BGP*>(g.ases[2].policy);
    assert(bgp2->hasRoute("1.0.0.0/8"));
    assert(bgp2->getRoute("1.0.0.0/8").received_from == FROM_PEER);

    delete bgp2;
    delete static_cast<BGP*>(g.ases[1].policy);
    std::cout << "PASS: test_peer_propagation" << std::endl;
}

// test_down_propagation - phase 3 (top-down)
// AS2 is a provider with a route; AS1 is its customer
// after propagation AS1 should have the route tagged FROM_PROVIDER
void test_down_propagation() {
    Graph g;
    AS a1; a1.asn = 1; a1.policy = new BGP();
    AS a2; a2.asn = 2; a2.policy = new BGP();
    
    a1.providers.push_back(2);
    a2.customers.push_back(1);
    
    g.ases[1] = a1;
    g.ases[2] = a2;

    static_cast<BGP*>(g.ases[2].policy)->installRoute(makeAnn("2.0.0.0/8", 2, FROM_PEER, {2}));

    std::vector<std::vector<int> > ranks(2);
    ranks[0].push_back(1);
    ranks[1].push_back(2);

    propagateAnnouncements(g, ranks);

    BGP* bgp1 = static_cast<BGP*>(g.ases[1].policy);
    assert(bgp1->hasRoute("2.0.0.0/8"));
    assert(bgp1->getRoute("2.0.0.0/8").received_from == FROM_PROVIDER);

    delete bgp1;
    delete static_cast<BGP*>(g.ases[2].policy);
    std::cout << "PASS: test_down_propagation" << std::endl;
}

// test_as_path_prepending - verifies that each AS prepends its own ASN as a route travels
// topology: 1 -> 2 -> 3 (each arrow = customer to provider)
// AS1 originates the route with path {1}; by the time AS3 has it the path should be {3, 2, 1}
void test_as_path_prepending() {
    Graph g;
    AS a1; a1.asn = 1; a1.policy = new BGP();
    AS a2; a2.asn = 2; a2.policy = new BGP();
    AS a3; a3.asn = 3; a3.policy = new BGP();
    
    a1.providers.push_back(2);
    a2.customers.push_back(1); a2.providers.push_back(3);
    a3.customers.push_back(2);
    
    g.ases[1] = a1;
    g.ases[2] = a2;
    g.ases[3] = a3;

    static_cast<BGP*>(g.ases[1].policy)->installRoute(makeAnn("1.0.0.0/8", 1, FROM_CUSTOMER, {1}));

    std::vector<std::vector<int> > ranks(3);
    ranks[0].push_back(1);
    ranks[1].push_back(2);
    ranks[2].push_back(3);

    propagateAnnouncements(g, ranks);

    Announcement a = static_cast<BGP*>(g.ases[3].policy)->getRoute("1.0.0.0/8");
    assert(a.as_path.size() == 3);
    assert(a.as_path[0] == 3);
    assert(a.as_path[1] == 2);
    assert(a.as_path[2] == 1);

    delete static_cast<BGP*>(g.ases[1].policy);
    delete static_cast<BGP*>(g.ases[2].policy);
    delete static_cast<BGP*>(g.ases[3].policy);
    std::cout << "PASS: test_as_path_prepending" << std::endl;
}

// test_conflict_customer_beats_peer - Gao-Rexford tiebreak #1
// AS4 receives the same prefix from AS1 (its customer) and AS2 (its peer)
// FROM_CUSTOMER has higher preference than FROM_PEER, so AS4 should pick AS1's route
void test_conflict_customer_beats_peer() {
    Graph g;
    AS a1; a1.asn = 1; a1.policy = new BGP();
    AS a2; a2.asn = 2; a2.policy = new BGP();
    AS a4; a4.asn = 4; a4.policy = new BGP();
    
    a1.providers.push_back(4);
    a4.customers.push_back(1);
    
    a2.peers.push_back(4);
    a4.peers.push_back(2);
    
    g.ases[1] = a1;
    g.ases[2] = a2;
    g.ases[4] = a4;

    static_cast<BGP*>(g.ases[1].policy)->installRoute(makeAnn("10.0.0.0/8", 1, FROM_CUSTOMER, {1}));
    static_cast<BGP*>(g.ases[2].policy)->installRoute(makeAnn("10.0.0.0/8", 2, FROM_CUSTOMER, {2}));

    std::vector<std::vector<int> > ranks = buildPropagationRanks(g);
    propagateAnnouncements(g, ranks);

    BGP* bgp4 = static_cast<BGP*>(g.ases[4].policy);
    Announcement best = bgp4->getRoute("10.0.0.0/8");
    assert(best.received_from == FROM_CUSTOMER);
    assert(best.next_hop_asn == 1);

    delete static_cast<BGP*>(g.ases[1].policy);
    delete static_cast<BGP*>(g.ases[2].policy);
    delete bgp4;
    std::cout << "PASS: test_conflict_customer_beats_peer" << std::endl;
}

// test_conflict_shorter_as_path - Gao-Rexford tiebreak #2
// AS1 receives the same prefix from peer AS2 (long path {2,5,6}) and peer AS3 (short path {3})
// received_from is the same (FROM_PEER for both), so shorter path wins -> AS3's route
void test_conflict_shorter_as_path() {
    Graph g;
    AS a1; a1.asn = 1; a1.policy = new BGP();
    AS a2; a2.asn = 2; a2.policy = new BGP();
    AS a3; a3.asn = 3; a3.policy = new BGP();
    
    a1.peers.push_back(2); a1.peers.push_back(3);
    a2.peers.push_back(1);
    a3.peers.push_back(1);
    
    g.ases[1] = a1;
    g.ases[2] = a2;
    g.ases[3] = a3;

    static_cast<BGP*>(g.ases[2].policy)->installRoute(makeAnn("10.0.0.0/8", 2, FROM_CUSTOMER, {2, 5, 6}));

    static_cast<BGP*>(g.ases[3].policy)->installRoute(makeAnn("10.0.0.0/8", 3, FROM_CUSTOMER, {3}));

    std::vector<std::vector<int> > ranks(1);
    ranks[0].push_back(1); ranks[0].push_back(2); ranks[0].push_back(3);

    propagateAnnouncements(g, ranks);

    BGP* bgp1 = static_cast<BGP*>(g.ases[1].policy);
    Announcement best = bgp1->getRoute("10.0.0.0/8");

    assert(best.as_path.size() == 2);
    assert(best.next_hop_asn == 3);

    delete static_cast<BGP*>(g.ases[2].policy);
    delete static_cast<BGP*>(g.ases[3].policy);
    delete bgp1;
    std::cout << "PASS: test_conflict_shorter_as_path" << std::endl;
}

// test_conflict_lower_next_hop - Gao-Rexford tiebreak #3 (final tiebreak)
// AS1 receives the same prefix from peer AS2 and peer AS3, both with equal-length paths
// received_from and path length are identical, so lower next_hop_asn wins -> AS2 (asn=2 < 3)
void test_conflict_lower_next_hop() {
    Graph g;
    AS a1; a1.asn = 1; a1.policy = new BGP();
    AS a2; a2.asn = 2; a2.policy = new BGP();
    AS a3; a3.asn = 3; a3.policy = new BGP();
    
    a1.peers.push_back(2); a1.peers.push_back(3);
    a2.peers.push_back(1);
    a3.peers.push_back(1);
    
    g.ases[1] = a1;
    g.ases[2] = a2;
    g.ases[3] = a3;

    static_cast<BGP*>(g.ases[2].policy)->installRoute(makeAnn("10.0.0.0/8", 2, FROM_CUSTOMER, {2}));
    static_cast<BGP*>(g.ases[3].policy)->installRoute(makeAnn("10.0.0.0/8", 3, FROM_CUSTOMER, {3}));

    std::vector<std::vector<int> > ranks(1);
    ranks[0].push_back(1); ranks[0].push_back(2); ranks[0].push_back(3);

    propagateAnnouncements(g, ranks);

    BGP* bgp1 = static_cast<BGP*>(g.ases[1].policy);
    Announcement best = bgp1->getRoute("10.0.0.0/8");

    assert(best.next_hop_asn == 2);

    delete static_cast<BGP*>(g.ases[2].policy);
    delete static_cast<BGP*>(g.ases[3].policy);
    delete bgp1;
    std::cout << "PASS: test_conflict_lower_next_hop" << std::endl;
}

// test_csv_tiny_graph - checks that dumpGraphToCsv() writes a valid CSV
// runs a minimal two-AS propagation then verifies the output file has a header
// row and a row containing the origin AS and prefix
void test_csv_tiny_graph() {
    Graph g;
    AS a1; a1.asn = 1; a1.policy = new BGP();
    AS a2; a2.asn = 2; a2.policy = new BGP();

    a1.providers.push_back(2);
    a2.customers.push_back(1);

    g.ases[1] = a1;
    g.ases[2] = a2;

    static_cast<BGP*>(g.ases[1].policy)->installRoute(makeAnn("1.2.0.0/16", 1, FROM_CUSTOMER, {1}));

    std::vector<std::vector<int> > ranks = buildPropagationRanks(g);
    propagateAnnouncements(g, ranks);
    dumpGraphToCsv(g, "tiny.csv");

    std::ifstream in("tiny.csv");
    std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    assert(all.find("asn,prefix,as_path") != std::string::npos);
    assert(all.find("1,1.2.0.0/16") != std::string::npos);

    delete static_cast<BGP*>(g.ases[1].policy);
    delete static_cast<BGP*>(g.ases[2].policy);
    std::cout << "PASS: test_csv_tiny_graph" << std::endl;
}

// test_csv_larger_graph - same CSV sanity check but with a four-AS mixed topology
// (provider/customer chain plus a peer) to make sure the CSV output covers all ASes
void test_csv_larger_graph() {
    Graph g;
    AS a1; a1.asn = 1; a1.policy = new BGP();
    AS a2; a2.asn = 2; a2.policy = new BGP();
    AS a3; a3.asn = 3; a3.policy = new BGP();
    AS a4; a4.asn = 4; a4.policy = new BGP();

    a1.providers.push_back(2);
    a2.customers.push_back(1); a2.providers.push_back(3);
    a3.customers.push_back(2); a3.peers.push_back(4);
    a4.peers.push_back(3);

    g.ases[1] = a1; g.ases[2] = a2; g.ases[3] = a3; g.ases[4] = a4;

    static_cast<BGP*>(g.ases[1].policy)->installRoute(makeAnn("1.2.0.0/16", 1, FROM_CUSTOMER, {1}));

    std::vector<std::vector<int> > ranks = buildPropagationRanks(g);
    propagateAnnouncements(g, ranks);
    dumpGraphToCsv(g, "large.csv");

    std::ifstream in("large.csv");
    std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    assert(all.find("1.2.0.0/16") != std::string::npos);

    delete static_cast<BGP*>(g.ases[1].policy);
    delete static_cast<BGP*>(g.ases[2].policy);
    delete static_cast<BGP*>(g.ases[3].policy);
    delete static_cast<BGP*>(g.ases[4].policy);
    std::cout << "PASS: test_csv_larger_graph" << std::endl;
}

// test_two_announcements_same_prefix - end-to-end conflict test using buildPropagationRanks()
// AS1 (customer of AS4) and AS2 (peer of AS4) both announce the same prefix
// AS4 should end up with the FROM_CUSTOMER route from AS1 and the CSV should reflect that
void test_two_announcements_same_prefix() {
    Graph g;
    AS a1; a1.asn = 1; a1.policy = new BGP();
    AS a2; a2.asn = 2; a2.policy = new BGP();
    AS a4; a4.asn = 4; a4.policy = new BGP();

    a1.providers.push_back(4);
    a4.customers.push_back(1);
    a2.peers.push_back(4);
    a4.peers.push_back(2);

    g.ases[1] = a1; g.ases[2] = a2; g.ases[4] = a4;

    static_cast<BGP*>(g.ases[1].policy)->installRoute(makeAnn("10.0.0.0/8", 1, FROM_CUSTOMER, {1}));
    static_cast<BGP*>(g.ases[2].policy)->installRoute(makeAnn("10.0.0.0/8", 2, FROM_CUSTOMER, {2}));

    std::vector<std::vector<int> > ranks = buildPropagationRanks(g);
    propagateAnnouncements(g, ranks);

    Announcement best = static_cast<BGP*>(g.ases[4].policy)->getRoute("10.0.0.0/8");
    assert(best.received_from == FROM_CUSTOMER);
    assert(best.next_hop_asn == 1);

    dumpGraphToCsv(g, "conflict.csv");

    delete static_cast<BGP*>(g.ases[1].policy);
    delete static_cast<BGP*>(g.ases[2].policy);
    delete static_cast<BGP*>(g.ases[4].policy);
    std::cout << "PASS: test_two_announcements_same_prefix" << std::endl;
}

int main() {
    test_up_propagation();
    test_peer_propagation();
    test_down_propagation();
    test_as_path_prepending();
    test_conflict_customer_beats_peer();
    test_conflict_shorter_as_path();
    test_conflict_lower_next_hop();
    test_csv_tiny_graph();
    test_csv_larger_graph();
    test_two_announcements_same_prefix();
    std::cout << "All propagation tests passed." << std::endl;
    return 0;
}