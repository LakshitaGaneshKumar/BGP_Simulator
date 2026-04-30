#ifndef ROV_POLICY_HPP
#define ROV_POLICY_HPP

#include <string>
#include "bgp_policy.hpp"
#include "graph.hpp"

class ROV : public BGP {
public:
    void receiveAnnouncement(const Announcement& ann) override {
        if (ann.rov_invalid) {
            return;
        }
        BGP::receiveAnnouncement(ann);
    }
};

void applyRovPolicies(Graph& graph, const std::string& rovFilePath);

#endif