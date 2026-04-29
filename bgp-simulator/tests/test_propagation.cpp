#include <cassert>
#include <iostream>
#include "../include/as.hpp"
#include "../include/bgp_policy.hpp"

static Announcement makeAnn(const std::string& prefix, int hop, int from, const std::vector<int>& path) {
    Announcement a;
    a.prefix = prefix;
    a.next_hop_asn = hop;
    a.received_from = from;
    a.as_path = path;
    return a;
}

void test_receive_queue_same_prefix() {
    BGP bgp;

    Announcement a1 = makeAnn("8.8.8.0/24", 15169, FROM_CUSTOMER, std::vector<int>{15169});
    Announcement a2 = makeAnn("8.8.8.0/24", 42, FROM_PEER, std::vector<int>{42, 15169});

    bgp.receiveAnnouncement(a1);
    bgp.receiveAnnouncement(a2);

    assert(bgp.received_queue.count("8.8.8.0/24") == 1);
    assert(bgp.received_queue["8.8.8.0/24"].size() == 2);
    std::cout << "PASS: test_receive_queue_same_prefix" << std::endl;
}

void test_receive_queue_different_prefixes() {
    BGP bgp;

    Announcement a1 = makeAnn("8.8.8.0/24", 15169, FROM_CUSTOMER, std::vector<int>{15169});
    Announcement a2 = makeAnn("2001:db8::/32", 64512, FROM_PROVIDER, std::vector<int>{64512});

    bgp.receiveAnnouncement(a1);
    bgp.receiveAnnouncement(a2);

    assert(bgp.received_queue.count("8.8.8.0/24") == 1);
    assert(bgp.received_queue.count("2001:db8::/32") == 1);
    assert(bgp.received_queue["8.8.8.0/24"].size() == 1);
    assert(bgp.received_queue["2001:db8::/32"].size() == 1);
    std::cout << "PASS: test_receive_queue_different_prefixes" << std::endl;
}

void test_install_and_lookup_route() {
    BGP bgp;

    Announcement best = makeAnn("1.2.3.0/24", 777, FROM_CUSTOMER, std::vector<int>{3, 777});
    bgp.installRoute(best);

    assert(bgp.hasRoute("1.2.3.0/24") == true);
    Announcement got = bgp.getRoute("1.2.3.0/24");
    assert(got.prefix == "1.2.3.0/24");
    assert(got.next_hop_asn == 777);
    assert(got.received_from == FROM_CUSTOMER);
    assert(got.as_path.size() == 2);
    assert(got.as_path[0] == 3);
    assert(got.as_path[1] == 777);

    assert(bgp.hasRoute("9.9.9.0/24") == false);
    std::cout << "PASS: test_install_and_lookup_route" << std::endl;
}

void test_policy_pointer_on_as() {
    AS node;
    node.asn = 10;

    BGP* bgp = new BGP();
    node.policy = bgp;

    Announcement a = makeAnn("10.0.0.0/8", 10, FROM_CUSTOMER, std::vector<int>{10});
    node.policy->receiveAnnouncement(a);

    assert(bgp->received_queue.count("10.0.0.0/8") == 1);
    assert(bgp->received_queue["10.0.0.0/8"].size() == 1);

    delete bgp;
    node.policy = 0;

    std::cout << "PASS: test_policy_pointer_on_as" << std::endl;
}

int main() {
    test_receive_queue_same_prefix();
    test_receive_queue_different_prefixes();
    test_install_and_lookup_route();
    test_policy_pointer_on_as();

    std::cout << "All policy/announcement tests passed." << std::endl;
    return 0;
}