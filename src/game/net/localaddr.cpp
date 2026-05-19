#include "localaddr.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#endif

#include <cstring>

std::vector<LocalAddress> getLocalAddresses() {
  std::vector<LocalAddress> result;

#ifdef _WIN32
  ULONG bufLen = 15000;
  PIP_ADAPTER_ADDRESSES addrs = (PIP_ADAPTER_ADDRESSES)malloc(bufLen);
  if (!addrs) return result;

  DWORD ret = GetAdaptersAddresses(AF_UNSPEC,
      GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
      nullptr, addrs, &bufLen);
  if (ret == ERROR_BUFFER_OVERFLOW) {
    free(addrs);
    addrs = (PIP_ADAPTER_ADDRESSES)malloc(bufLen);
    if (!addrs) return result;
    ret = GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr, addrs, &bufLen);
  }

  if (ret == NO_ERROR) {
    for (auto* adapter = addrs; adapter; adapter = adapter->Next) {
      if (adapter->OperStatus != IfOperStatusUp) continue;
      if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

      for (auto* ua = adapter->FirstUnicastAddress; ua; ua = ua->Next) {
        char buf[INET6_ADDRSTRLEN] = {};
        auto* sa = ua->Address.lpSockaddr;

        if (sa->sa_family == AF_INET) {
          auto* sin = (sockaddr_in*)sa;
          inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
          result.push_back({buf, false});
        } else if (sa->sa_family == AF_INET6) {
          auto* sin6 = (sockaddr_in6*)sa;
          // Skip link-local (fe80::)
          if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) continue;
          inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf));
          result.push_back({buf, true});
        }
      }
    }
  }
  free(addrs);

#else
  struct ifaddrs* ifas = nullptr;
  if (getifaddrs(&ifas) != 0) return result;

  for (auto* ifa = ifas; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr) continue;
    if (!(ifa->ifa_flags & IFF_UP)) continue;
    if (ifa->ifa_flags & IFF_LOOPBACK) continue;

    char buf[INET6_ADDRSTRLEN] = {};

    if (ifa->ifa_addr->sa_family == AF_INET) {
      auto* sin = (sockaddr_in*)ifa->ifa_addr;
      inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
      result.push_back({buf, false});
    } else if (ifa->ifa_addr->sa_family == AF_INET6) {
      auto* sin6 = (sockaddr_in6*)ifa->ifa_addr;
      // Skip link-local (fe80::)
      if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) continue;
      inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf));
      result.push_back({buf, true});
    }
  }
  freeifaddrs(ifas);
#endif

  return result;
}
