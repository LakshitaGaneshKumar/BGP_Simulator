// test_rov.cpp
// unit tests for Route Origin Validation (ROV) policy behavior
//
// ROV is a security extension to BGP where an AS rejects route announcements
// that are marked rov_invalid (e.g. the origin ASN doesn't match the registered
// RPKI prefix-origin pair). A plain BGP AS ignores the rov_invalid flag entirely.
//
// test list:
//   test_rov_drops_invalid_in_receive  - ROV silently drops rov_invalid=true on receive
//   test_bgp_accepts_invalid           - plain BGP queues rov_invalid=true without filtering
//   test_rov_in_propagation            - ROV AS never installs an invalid route during full propagation

#include <cassert>
#include <iostream>

#include "../include/as.hpp"
#include "../include/announcement.hpp"
#include "../include/bgp_policy.hpp"
#include "../include/rov_policy.hpp"
#include "../include/simulator.hpp"

// makeAnn - helper to build an Announcement with the rov_invalid flag set
// invalid=true simulates a route whose origin ASN doesn't match the RPKI record
static Announcement makeAnn(const std::string& prefix, int hop, int from, bool invalid) {
    Announcement a;
    a.prefix = prefix;
    a.next_hop_asn = hop;
    a.received_from = from;
    a.as_path = std::vector<int>{hop};
    a.rov_invalid = invalid;
    return a;
}

// test_rov_drops_invalid_in_receive - sends one invalid and one valid announcement
// to a ROV policy's receiveAnnouncement(); only the valid one should end up in the queue
void test_rov_drops_invalid_in_receive() {
    ROV rov;
    Announcement invalid = makeAnn("1.2.0.0/16", 666, FROM_PEER, true);
    Announcement valid   = makeAnn("1.2.0.0/16", 777, FROM_CUSTOMER, false);

    rov.receiveAnnouncement(invalid); // should be silently dropped
    rov.receiveAnnouncement(valid);   // should be queued

    assert(rov.received_queue["1.2.0.0/16"].size() == 1);
    assert(rov.received_queue["1.2.0.0/16"][0].next_hop_asn == 777);
    std::cout << "PASS: test_rov_drops_invalid_in_receive" << std::endl;
}

// test_bgp_accepts_invalid - confirms that a plain BGP AS does NOT filter rov_invalid routes
// the invalid announcement should still appear in the received_queue
void test_bgp_accepts_invalid() {
    BGP bgp;
    Announcement invalid = makeAnn("1.2.0.0/16", 666, FROM_PEER, true);
    bgp.receiveAnnouncement(invalid);

    assert(bgp.received_queue["1.2.0.0/16"].size() == 1); // BGP doesn't care about rov_invalid
    std::cout << "PASS: test_bgp_accepts_invalid" << std::endl;
}

// test_rov_in_propagation - integration test: AS1 (BGP) originates a hijacked/invalid route
// and AS2 (ROV) is a peer of AS1; after full propagation AS2 should have no route for the prefix
// because ROV dropped it on receive
void test_rov_in_propagation() {
    Graph g;

    AS a1; a1.asn = 1; a1.policy = new BGP(); // plain BGP, will accept and forward the invalid route
    AS a2; a2.asn = 2; a2.policy = new ROV(); // ROV enforcer, should reject it

    a1.peers.push_back(2);
    a2.peers.push_back(1);

    g.ases[1] = a1;
    g.ases[2] = a2;

    // seed a route flagged as rov_invalid (simulating a prefix hijack)
    Announcement hijack = makeAnn("9.9.0.0/16", 666, FROM_CUSTOMER, true);
    static_cast<BGP*>(g.ases[1].policy)->installRoute(hijack);

    std::vector<std::vector<int> > ranks(1);
    ranks[0].push_back(1);
    ranks[0].push_back(2);

    propagateAnnouncements(g, ranks);

    // AS2 is ROV so it should have dropped the hijack — local_rib should be empty
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