#include <cassert>
#include <iostream>

#include "../include/as.hpp"
#include "../include/announcement.hpp"
#include "../include/bgp_policy.hpp"
#include "../include/rov_policy.hpp"
#include "../include/simulator.hpp"

static Announcement makeAnn(const std::string& prefix, int hop, int from, bool invalid) {
    Announcement a;
    a.prefix = prefix;
    a.next_hop_asn = hop;
    a.received_from = from;
    a.as_path = std::vector<int>{hop};
    a.rov_invalid = invalid;
    return a;
}

void test_rov_drops_invalid_in_receive() {
    ROV rov;
    Announcement invalid = makeAnn("1.2.0.0/16", 666, FROM_PEER, true);
    Announcement valid = makeAnn("1.2.0.0/16", 777, FROM_CUSTOMER, false);

    rov.receiveAnnouncement(invalid);
    rov.receiveAnnouncement(valid);

    assert(rov.received_queue["1.2.0.0/16"].size() == 1);
    assert(rov.received_queue["1.2.0.0/16"][0].next_hop_asn == 777);
    std::cout << "PASS: test_rov_drops_invalid_in_receive" << std::endl;
}

void test_bgp_accepts_invalid() {
    BGP bgp;
    Announcement invalid = makeAnn("1.2.0.0/16", 666, FROM_PEER, true);
    bgp.receiveAnnouncement(invalid);

    assert(bgp.received_queue["1.2.0.0/16"].size() == 1);
    std::cout << "PASS: test_bgp_accepts_invalid" << std::endl;
}

void test_rov_in_propagation() {
    Graph g;

    AS a1; a1.asn = 1; a1.policy = new BGP();
    AS a2; a2.asn = 2; a2.policy = new ROV();

    a1.peers.push_back(2);
    a2.peers.push_back(1);

    g.ases[1] = a1;
    g.ases[2] = a2;

    Announcement hijack = makeAnn("9.9.0.0/16", 666, FROM_CUSTOMER, true);
    static_cast<BGP*>(g.ases[1].policy)->installRoute(hijack);

    std::vector<std::vector<int> > ranks(1);
    ranks[0].push_back(1);
    ranks[0].push_back(2);

    propagateAnnouncements(g, ranks);

    BGP* p2 = static_cast<BGP*>(g.ases[2].policy);
    assert(!p2->hasRoute("9.9.0.0/16"));

    delete static_cast<BGP*>(g.ases[1].policy);
    delete static_cast<BGP*>(g.ases[2].policy);
    std::cout << "PASS: test_rov_in_propagation" << std::endl;
}

int main() {
    test_rov_drops_invalid_in_receive();
    test_bgp_accepts_invalid();
    test_rov_in_propagation();
    std::cout << "All ROV tests passed." << std::endl;
    return 0;
}