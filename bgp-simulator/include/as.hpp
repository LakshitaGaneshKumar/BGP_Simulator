#ifndef AS_HPP
#define AS_HPP

#include <vector>

struct AS {
    int asn;
    std::vector<int> providers;
    std::vector<int> customers;
    std::vector<int> peers;
};

#endif
