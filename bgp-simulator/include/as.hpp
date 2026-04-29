#ifndef AS_HPP
#define AS_HPP

#include <vector>

class Policy;

struct AS {
    int asn;
    std::vector<int> providers;
    std::vector<int> customers;
    std::vector<int> peers;
    Policy* policy;
    int propagation_rank;

    AS() : asn(0), policy(0), propagation_rank(-1) {}
};

#endif