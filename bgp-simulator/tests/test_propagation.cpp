#include <cassert>
#include <iostream>
#include "../include/graph.hpp"
#include "../include/as.hpp"
#include "../include/announcement.hpp"
#include "../include/bgp_policy.hpp"
#include "../include/simulator.hpp"

static Announcement makeAnn(const std::string& prefix, int hop, int from, const std::vector<int>& path) {
    Announcement a;
    a.prefix = prefix;
    a.next_hop_asn = hop;
    a.received_from = from;
    a.as_path = path;
    return a;
}

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

int main() {
    test_up_propagation();
    test_peer_propagation();
    test_down_propagation();
    test_as_path_prepending();
    std::cout << "All propagation tests passed." << std::endl;
    return 0;
}