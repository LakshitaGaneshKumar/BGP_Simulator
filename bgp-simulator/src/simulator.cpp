// simulator.cpp
// core BGP route propagation logic
// given a ranked graph (from propagation_rank.cpp), this runs the actual
// simulation in three phases following Gao-Rexford rules:
//
//   phase 1 (bottom-up): customers send to providers, rank 0 -> rank N
//   phase 2 (peer):      all ASes send to peers
//   phase 3 (top-down):  providers send to customers, rank N -> rank 0
//
// Gao-Rexford route preference order (highest to lowest):
//   1. customer routes  (FROM_CUSTOMER) - best, earns money
//   2. peer routes      (FROM_PEER)
//   3. provider routes  (FROM_PROVIDER) - worst, costs money
// within same preference: prefer shorter AS path, then lower next-hop ASN as tiebreak

#include <map>
#include <string>
#include <vector>

#include "../include/simulator.hpp"
#include "../include/bgp_policy.hpp"
#include "../include/announcement.hpp"
#include "../include/as.hpp"

// ensureBGP - returns the BGP policy for an AS, creating one if it doesn't have one yet
// (peer/provider ASes that aren't origin ASes may not have a policy set before propagation starts)
static BGP* ensureBGP(AS& node) {
    if (!node.policy) {
        node.policy = new BGP();
    }
    return static_cast<BGP*>(node.policy);
}

// isBetterAnnouncement - returns true if candidate should replace the currently stored route
// follows Gao-Rexford preference: received_from > path length > next_hop_asn tiebreak
static bool isBetterAnnouncement(const Announcement& candidate, const Announcement& stored) {
    // higher received_from value = more preferred (customer > peer > provider)
    if (candidate.received_from != stored.received_from) {
        return candidate.received_from > stored.received_from;
    }

    // prefer shorter AS paths (fewer hops to the origin)
    if (candidate.as_path.size() != stored.as_path.size()) {
        return candidate.as_path.size() < stored.as_path.size();
    }
    
    // tiebreak: lower next-hop ASN wins (arbitrary but deterministic)
    return candidate.next_hop_asn < stored.next_hop_asn;
}

// processAndStoreForAS - drains the received_queue for an AS and installs the best route
// for each prefix into its local_rib
// prepends the AS's own ASN to the path before comparing (so the path is always current)
static void processAndStoreForAS(AS& node) {
    BGP* bgp = ensureBGP(node);

    for (std::map<std::string, std::vector<Announcement> >::iterator it = bgp->received_queue.begin();
         it != bgp->received_queue.end(); ++it) {
        const std::string& prefix = it->first;
        std::vector<Announcement>& incoming = it->second;

        for (size_t i = 0; i < incoming.size(); ++i) {
            Announcement candidate = incoming[i];

            // prepend this AS to the path so the path reflects the full route
            candidate.as_path.insert(candidate.as_path.begin(), node.asn);

            if (bgp->hasRoute(prefix)) {
                Announcement stored = bgp->getRoute(prefix);
                if (isBetterAnnouncement(candidate, stored)) {
                    bgp->installRoute(candidate);
                }
            } else {
                // no route for this prefix yet, install whatever we got
                bgp->installRoute(candidate);
            }
        }
    }

    bgp->received_queue.clear();
}

// sendLocalRibToProviders - sends all routes in an AS's local RIB up to its providers
// tagged as FROM_CUSTOMER so providers know to prefer these routes (Gao-Rexford: customer = best)
static void sendLocalRibToProviders(Graph& graph, AS& sender) {
    BGP* senderBGP = ensureBGP(sender);

    for (std::map<std::string, Announcement>::iterator it = senderBGP->local_rib.begin();
         it != senderBGP->local_rib.end(); ++it) {
        Announcement out = it->second;
        out.next_hop_asn = sender.asn;
        out.received_from = FROM_CUSTOMER; // from the provider's perspective, this sender is its customer

        for (size_t i = 0; i < sender.providers.size(); ++i) {
            AS& provider = graph.ases[sender.providers[i]];
            BGP* providerBGP = ensureBGP(provider);
            providerBGP->receiveAnnouncement(out);
        }
    }
}

// sendLocalRibToPeers - sends all routes to peer ASes, tagged as FROM_PEER
// note: only customer-learned routes should technically be sent to peers under strict Gao-Rexford,
// but here we send the full RIB for simplicity
static void sendLocalRibToPeers(Graph& graph, AS& sender) {
    BGP* senderBGP = ensureBGP(sender);

    for (std::map<std::string, Announcement>::iterator it = senderBGP->local_rib.begin();
         it != senderBGP->local_rib.end(); ++it) {
        Announcement out = it->second;
        out.next_hop_asn = sender.asn;
        out.received_from = FROM_PEER;

        for (size_t i = 0; i < sender.peers.size(); ++i) {
            AS& peer = graph.ases[sender.peers[i]];
            BGP* peerBGP = ensureBGP(peer);
            peerBGP->receiveAnnouncement(out);
        }
    }
}

// sendLocalRibToCustomers - sends all routes down to customer ASes, tagged as FROM_PROVIDER
// this is phase 3 (top-down), so providers are pushing routes to customers
static void sendLocalRibToCustomers(Graph& graph, AS& sender) {
    BGP* senderBGP = ensureBGP(sender);

    for (std::map<std::string, Announcement>::iterator it = senderBGP->local_rib.begin();
         it != senderBGP->local_rib.end(); ++it) {
        Announcement out = it->second;
        out.next_hop_asn = sender.asn;
        out.received_from = FROM_PROVIDER; // from the customer's perspective, this sender is its provider

        for (size_t i = 0; i < sender.customers.size(); ++i) {
            AS& customer = graph.ases[sender.customers[i]];
            BGP* customerBGP = ensureBGP(customer);
            customerBGP->receiveAnnouncement(out);
        }
    }
}

// propagateAnnouncements - runs the full three-phase BGP propagation
//
// phase 1: bottom-up (customer -> provider)
//   process ranks in ascending order; each rank sends to providers, then
//   the next rank up processes its received queue
//
// phase 2: peer exchange
//   all ASes send their RIB to peers and process peer announcements
//
// phase 3: top-down (provider -> customer)
//   process ranks in descending order; each rank sends to customers, then
//   the rank below processes its received queue
void propagateAnnouncements(Graph& graph, const std::vector<std::vector<int> >& ranks) {
    if (ranks.empty()) {
        return;
    }

    // phase 1: bottom-up - customers send routes up to providers
    for (size_t r = 0; r + 1 < ranks.size(); ++r) {
        const std::vector<int>& curRank = ranks[r];
        for (size_t i = 0; i < curRank.size(); ++i) {
            AS& sender = graph.ases[curRank[i]];
            sendLocalRibToProviders(graph, sender);
        }

        // once a rank has sent, the next rank up can process what it received
        const std::vector<int>& nextRank = ranks[r + 1];
        for (size_t i = 0; i < nextRank.size(); ++i) {
            AS& receiver = graph.ases[nextRank[i]];
            processAndStoreForAS(receiver);
        }
    }

    // phase 2: peer exchange - all ASes send to peers and process peer routes
    for (std::map<int, AS>::iterator it = graph.ases.begin(); it != graph.ases.end(); ++it) {
        sendLocalRibToPeers(graph, it->second);
    }
    for (std::map<int, AS>::iterator it = graph.ases.begin(); it != graph.ases.end(); ++it) {
        processAndStoreForAS(it->second);
    }

    // phase 3: top-down - providers send routes down to customers
    for (int r = (int)ranks.size() - 1; r - 1 >= 0; --r) {
        const std::vector<int>& curRank = ranks[(size_t)r];
        for (size_t i = 0; i < curRank.size(); ++i) {
            AS& sender = graph.ases[curRank[i]];
            sendLocalRibToCustomers(graph, sender);
        }

        // once a rank has sent, the rank below processes what it received
        const std::vector<int>& nextRankDown = ranks[(size_t)(r - 1)];
        for (size_t i = 0; i < nextRankDown.size(); ++i) {
            AS& receiver = graph.ases[nextRankDown[i]];
            processAndStoreForAS(receiver);
        }
    }
}
