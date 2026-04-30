#ifndef ANNOUNCEMENT_HPP
#define ANNOUNCEMENT_HPP

#include <string>
#include <vector>

#define FROM_PROVIDER -1
#define FROM_PEER 0
#define FROM_CUSTOMER 1

struct Announcement {
    std::string prefix;
    std::vector<int> as_path;
    int next_hop_asn;
    int received_from;
    bool rov_invalid;

    Announcement() : next_hop_asn(0), received_from(FROM_PROVIDER), rov_invalid(false) {}
};

#endif