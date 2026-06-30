#pragma once

#include <string>

namespace recommendation {

struct HttpResponse {
  int status_code{0};
  std::string body;
};

bool HttpRequest(const std::string &registry_address, const std::string &method,
                 const std::string &path, const std::string &body,
                 HttpResponse *response);

}  // namespace recommendation
