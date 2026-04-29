#ifndef POLICY_HPP
#define POLICY_HPP

#include <string>
#include "announcement.hpp"

class Policy {
public:
    virtual ~Policy() {}

    virtual void receiveAnnouncement(const Announcement& ann) = 0;
    virtual bool hasRoute(const std::string& prefix) const = 0;
    virtual Announcement getRoute(const std::string& prefix) const = 0;
};

#endif