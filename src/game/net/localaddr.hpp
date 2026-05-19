#pragma once

#include <string>
#include <vector>

struct LocalAddress {
  std::string ip;
  bool isIPv6;
};

// Returns non-loopback local addresses (IPv4 and IPv6 link-local/global)
std::vector<LocalAddress> getLocalAddresses();
