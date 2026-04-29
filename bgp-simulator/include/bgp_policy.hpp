#ifndef BGP_POLICY_HPP
#define BGP_POLICY_HPP

#include <map>
#include <string>
#include <vector>
#include "policy.hpp"
#include "announcement.hpp"

class BGP : public Policy {
public:
    std::map<std::string, Announcement> local_rib;
    std::map<std::string, std::vector<Announcement> > received_queue;

    void receiveAnnouncement(const Announcement& ann) override {
        received_queue[ann.prefix].push_back(ann);
    }

    bool hasRoute(const std::string& prefix) const override {
        return local_rib.find(prefix) != local_rib.end();
    }

    Announcement getRoute(const std::string& prefix) const override {
        std::map<std::string, Announcement>::const_iterator it = local_rib.find(prefix);
        if (it != local_rib.end()) {
            return it->second;
        }
        return Announcement();
    }

    void installRoute(const Announcement& ann) {
        local_rib[ann.prefix] = ann;
    }
};

#endif