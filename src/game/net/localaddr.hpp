#pragma once

#include <string>
#include <vector>

struct LocalAddress {
  std::string ip;
  bool is_i_pv6;
};

// Returns non-loopback local addresses (IPv4 and IPv6 link-local/global)
std::vector<LocalAddress> GetLocalAddresses();
