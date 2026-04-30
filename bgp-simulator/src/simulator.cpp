#include <map>
#include <string>
#include <vector>

#include "../include/simulator.hpp"
#include "../include/bgp_policy.hpp"
#include "../include/announcement.hpp"
#include "../include/as.hpp"

static BGP* ensureBGP(AS& node) {
    if (!node.policy) {
        node.policy = new BGP();
    }
    return static_cast<BGP*>(node.policy);
}

static bool isBetterAnnouncement(const Announcement& candidate, const Announcement& stored) {
    if (candidate.received_from != stored.received_from) {
        return candidate.received_from > stored.received_from;
    }

    if (candidate.as_path.size() != stored.as_path.size()) {
        return candidate.as_path.size() < stored.as_path.size();
    }
    
    return candidate.next_hop_asn < stored.next_hop_asn;
}

static void processAndStoreForAS(AS& node) {
    BGP* bgp = ensureBGP(node);

    for (std::map<std::string, std::vector<Announcement> >::iterator it = bgp->received_queue.begin();
         it != bgp->received_queue.end(); ++it) {
        const std::string& prefix = it->first;
        std::vector<Announcement>& incoming = it->second;

        for (size_t i = 0; i < incoming.size(); ++i) {
            Announcement candidate = incoming[i];

            candidate.as_path.insert(candidate.as_path.begin(), node.asn);

            if (bgp->hasRoute(prefix)) {
                Announcement stored = bgp->getRoute(prefix);
                if (isBetterAnnouncement(candidate, stored)) {
                    bgp->installRoute(candidate);
                }
            } else {
                bgp->installRoute(candidate);
            }
        }
    }

    bgp->received_queue.clear();
}

static void sendLocalRibToProviders(Graph& graph, AS& sender) {
    BGP* senderBGP = ensureBGP(sender);

    for (std::map<std::string, Announcement>::iterator it = senderBGP->local_rib.begin();
         it != senderBGP->local_rib.end(); ++it) {
        Announcement out = it->second;
        out.next_hop_asn = sender.asn;
        out.received_from = FROM_CUSTOMER;

        for (size_t i = 0; i < sender.providers.size(); ++i) {
            AS& provider = graph.ases[sender.providers[i]];
            BGP* providerBGP = ensureBGP(provider);
            providerBGP->receiveAnnouncement(out);
        }
    }
}

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

static void sendLocalRibToCustomers(Graph& graph, AS& sender) {
    BGP* senderBGP = ensureBGP(sender);

    for (std::map<std::string, Announcement>::iterator it = senderBGP->local_rib.begin();
         it != senderBGP->local_rib.end(); ++it) {
        Announcement out = it->second;
        out.next_hop_asn = sender.asn;
        out.received_from = FROM_PROVIDER;

        for (size_t i = 0; i < sender.customers.size(); ++i) {
            AS& customer = graph.ases[sender.customers[i]];
            BGP* customerBGP = ensureBGP(customer);
            customerBGP->receiveAnnouncement(out);
        }
    }
}

void propagateAnnouncements(Graph& graph, const std::vector<std::vector<int> >& ranks) {
    if (ranks.empty()) {
        return;
    }

    for (size_t r = 0; r + 1 < ranks.size(); ++r) {
        const std::vector<int>& curRank = ranks[r];
        for (size_t i = 0; i < curRank.size(); ++i) {
            AS& sender = graph.ases[curRank[i]];
            sendLocalRibToProviders(graph, sender);
        }

        const std::vector<int>& nextRank = ranks[r + 1];
        for (size_t i = 0; i < nextRank.size(); ++i) {
            AS& receiver = graph.ases[nextRank[i]];
            processAndStoreForAS(receiver);
        }
    }

    for (std::map<int, AS>::iterator it = graph.ases.begin(); it != graph.ases.end(); ++it) {
        sendLocalRibToPeers(graph, it->second);
    }
    for (std::map<int, AS>::iterator it = graph.ases.begin(); it != graph.ases.end(); ++it) {
        processAndStoreForAS(it->second);
    }

    for (int r = (int)ranks.size() - 1; r - 1 >= 0; --r) {
        const std::vector<int>& curRank = ranks[(size_t)r];
        for (size_t i = 0; i < curRank.size(); ++i) {
            AS& sender = graph.ases[curRank[i]];
            sendLocalRibToCustomers(graph, sender);
        }

        const std::vector<int>& nextRankDown = ranks[(size_t)(r - 1)];
        for (size_t i = 0; i < nextRankDown.size(); ++i) {
            AS& receiver = graph.ases[nextRankDown[i]];
            processAndStoreForAS(receiver);
        }
    }
}