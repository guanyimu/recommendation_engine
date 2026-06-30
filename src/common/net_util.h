#pragma once

#include <string>

namespace recommendation {

constexpr int kDefaultBindMaxAttempts = 10;
constexpr char kLoopbackHost[] = "127.0.0.1";

bool SplitHostPort(const std::string &endpoint, std::string *host, int *port);
std::string FormatHostPort(const std::string &host, int port);

}  // namespace recommendation
