#include "net_util.h"

#include <string>

namespace recommendation {

bool SplitHostPort(const std::string &endpoint, std::string *host, int *port) {
  if (host == nullptr || port == nullptr) {
    return false;
  }
  const std::size_t colon = endpoint.rfind(':');
  if (colon == std::string::npos || colon == 0 || colon + 1 >= endpoint.size()) {
    return false;
  }
  *host = endpoint.substr(0, colon);
  try {
    const int p = std::stoi(endpoint.substr(colon + 1));
    if (p <= 0 || p > 65535) {
      return false;
    }
    *port = p;
    return true;
  } catch (...) {
    return false;
  }
}

std::string FormatHostPort(const std::string &host, int port) {
  return host + ':' + std::to_string(port);
}

}  // namespace recommendation
